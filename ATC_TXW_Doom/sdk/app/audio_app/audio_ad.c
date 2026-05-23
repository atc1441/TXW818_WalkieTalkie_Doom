#include "basic_include.h"
#include "osal/task.h"
#include "hal/auadc.h"
#include "audio_adc.h"
#include "stream_frame.h"
#include "t_queue.h"
#include "csi_kernel.h"

#ifdef PSRAM_HEAP
	#define ADUIO_MALLOC	os_malloc_psram
	#define ADUIO_FREE		os_free_psram
	#define AUDIONUM	(16)	//因为带了psram,如果需要容忍更多容错防止丢帧,这里可以增加更多节点
	#define AUDIOSRAM_NUM	(4)//sram的空间个数,所以单个节点不能太大,否则有可能有问题空间不够
#else
	#define ADUIO_MALLOC	os_malloc
	#define ADUIO_FREE		os_free
	#define AUDIONUM	(4)
#endif

#define MEAN_FILTER        1

#if MEAN_FILTER == 1
	#define MEAN_FILTER_SAMPLE_LEN	2
#else
	#define MEAN_FILTER_SAMPLE_LEN	0
#endif

#define AUDIOLEN	(1024)

#define SOFT_GAIN	(8)

static stream *global_audio_adc_s = NULL;
struct audio_ad_config;
typedef void *(*set_buf)(void *priv_el,void *el_point);
typedef void (*get_buf)(void *priv_el,void *el_point);

typedef int32 (*audio_ad_read)(struct audio_ad_config *audio, void* buf, uint32 len);

struct audio_ad_config
{
	int buf_size;
    struct auadc_device *adc;
	void *current_node;
	void *reg_node;
	void *priv_el;
	set_buf set_buf;
	get_buf get_buf;
    audio_ad_read irq_func;	
};

void *audio_custom_malloc(uint32_t size)
{
	return os_malloc(size);
}

void audio_custom_free(void *ptr)
{
	os_free(ptr);
}

void audio_adc_irq(uint32 irq, uint32 irq_data)
{
	struct audio_ad_config *audio_ad = (struct audio_ad_config *)irq_data;
    struct audio_ad_config *priv = (struct audio_ad_config*)audio_ad;
	void *buf;

  	if(irq == AUADC_IRQ_FLAG_HALF)
  	{
		if(!priv->reg_node)
		{
			buf = priv->set_buf(priv->priv_el,&priv->reg_node);
			//如果reg_node为NULL,则音频录音buf还是原来的buf
			if(priv->reg_node)
			{
				priv->irq_func(audio_ad , buf, priv->buf_size);
			}
		}
		//这里由于其他地方关闭中断导致错过了很多中断,这里需要处理异常数据(这些数据依然发送,实际已经不再正确的音频)
		else
		{
			//如果相等,证明上一次执行了full中断
			if(priv->current_node == priv->reg_node)
			{
				priv->current_node = NULL;
			}
			//不相等,说明可以发送一些节点,实际数据并不是所需要,但还是要管理buf
			else
			{
				priv->get_buf(priv->priv_el,priv->current_node);
				priv->current_node = priv->reg_node;
			}	
		}
  	}
  	else if(irq == AUADC_IRQ_FLAG_FULL)
  	{
		//半中断有配置新的buf,则将完成的录音帧通知应用层,如果为NULL,则代表没有配置新的buf,只能使用旧的buf
	  	if(priv->reg_node)
	  	{
			if(priv->current_node)
			{
				priv->get_buf(priv->priv_el,priv->current_node);
			}
			priv->current_node = priv->reg_node;
			priv->reg_node = NULL;
	  	}
  	}
}

void audio_adc_register(void *audio_hdl,void *priv_el,int play_size,set_buf audio_set_buf,get_buf audio_get_buf)
{
	struct audio_ad_config *priv = (struct audio_ad_config*)audio_hdl;
    priv->buf_size = play_size;
	priv->set_buf = audio_set_buf;
	priv->get_buf = audio_get_buf;
	priv->priv_el = priv_el;	
}

#ifdef PSRAM_HEAP
static void *audio_set_buf(void *priv_el,void *el_point)
{
	void *buf = NULL;
    stream *s = (stream *)priv_el;
	struct audio_adc_s *audio_priv = (struct audio_adc_s*)s->priv;
	struct tqueue_s *queue_data;

    queue_data = tqueue_pop(audio_priv->queue);
    struct tqueue_s **point = (struct tqueue_s**)el_point;
    if(queue_data)
    {
        buf = queue_data->data;
		buf = (uint16_t*)buf+MEAN_FILTER_SAMPLE_LEN;
    }
    *point = queue_data;
	return buf;	
}

static void audio_get_buf(void *priv_el,void *el_point)
{
    stream *s = (stream *)priv_el;
    struct audio_adc_s *self_priv = (struct audio_adc_s*)s->priv;
    struct tqueue_s *queue_data = (struct tqueue_s*)el_point;	
    int res;

	if(!queue_data)
	{
		_os_printf("%s:%d err\n",__FUNCTION__,__LINE__);
		return;
	}

	struct data_structure  *data = get_src_data_f(s);
	if(data)
	{
		data->priv = queue_data;
		set_stream_data_time(data,os_jiffies());
		res = csi_kernel_msgq_put(self_priv->adc_msgq,&data,0,0);
		//正常应该保证不进这里,如果进来代表任务没有获取队列,直接配置下一个buf导致的
		if(res)
		{
			_os_printf("P");
			force_del_data(data);
			tqueue_push(self_priv->queue,queue_data);
		}
	}
	//找不到节点,则queue_data需要放回到队列里面去
	else
	{
		tqueue_push(self_priv->queue,queue_data);
	}

	return;
	
}
#else
static void *audio_set_buf(void *priv_el,void *el_point)
{
	void *buf = NULL;
    stream *s = (stream *)priv_el;
    struct data_structure  *data;

    data = get_src_data_f(s);
    struct data_structure **point = (struct data_structure**)el_point;     
    if(data)
    {
        buf = get_stream_real_data(data);
		buf = (uint16_t*)buf+MEAN_FILTER_SAMPLE_LEN;
    }
    *point = data;
	return buf;	
}

static void audio_get_buf(void *priv_el,void *el_point)
{
    stream *s = (stream *)priv_el;
    struct audio_adc_s *self_priv = (struct audio_adc_s*)s->priv;
    struct data_structure *data = (struct data_structure*)el_point;
    int res;
	if(!data)
	{
		_os_printf("%s:%d err\n",__FUNCTION__,__LINE__);
		return;
	}
    set_stream_data_time(data,os_jiffies());
	res = csi_kernel_msgq_put(self_priv->adc_msgq,&data,0,0);
	//正常应该保证不进这里,如果进来代表任务没有获取队列,直接配置下一个buf导致的
	if(res)
	{
		_os_printf("P");
        force_del_data(data);
	}
	return;	
}
#endif

static void audio_deal_task(void *arg)
{
#if MEAN_FILTER == 1
	int16_t mean_filter_prev_buf[MEAN_FILTER_SAMPLE_LEN] = {0};
#endif
	int16_t *p_buf;	
	int res;
	int32_t temp32 = 0;
	uint32_t sample_len;
	struct data_structure *data;
	stream *s = (stream *)arg;
	struct audio_adc_s *self_priv = (struct audio_adc_s*)s->priv;
	#ifdef PSRAM_HEAP
	struct tqueue_s *queue_data;
	#endif
#if MEAN_FILTER == 1
	os_memset(mean_filter_prev_buf, 0, MEAN_FILTER_SAMPLE_LEN*2);
#endif	 
	while(1)
	{
		res = csi_kernel_msgq_get(self_priv->adc_msgq,&data,-1);
		if(!res)
		{
            //发送
			#ifdef PSRAM_HEAP
			queue_data = (struct tqueue_s *)data->priv;
            p_buf = queue_data->data;
			sample_len = get_stream_real_data_len(data)/2;
			#else
            p_buf = get_stream_real_data(data);
			sample_len = get_stream_real_data_len(data)/2;
			#endif
			
		#if MEAN_FILTER == 1
			os_memcpy(p_buf, mean_filter_prev_buf, MEAN_FILTER_SAMPLE_LEN*2);
        	for(uint32_t i=MEAN_FILTER_SAMPLE_LEN; i<(sample_len+MEAN_FILTER_SAMPLE_LEN); i++) {
				int16_t mean_value = ((int32_t)p_buf[i-MEAN_FILTER_SAMPLE_LEN]+p_buf[i]) >> 1;
				if( (p_buf[i-MEAN_FILTER_SAMPLE_LEN+1] > (p_buf[i-MEAN_FILTER_SAMPLE_LEN] + 3276)) &&
					(p_buf[i-MEAN_FILTER_SAMPLE_LEN+1] > (p_buf[i] + 3276)) &&
					(p_buf[i-MEAN_FILTER_SAMPLE_LEN+1] > (mean_value + 3276))) {
					p_buf[i-MEAN_FILTER_SAMPLE_LEN] = mean_value;
				}
				else {
					p_buf[i-MEAN_FILTER_SAMPLE_LEN] = p_buf[i-MEAN_FILTER_SAMPLE_LEN+1];
				}
			} 
			os_memcpy(mean_filter_prev_buf, p_buf+sample_len, MEAN_FILTER_SAMPLE_LEN*2);	
		#endif	

			for(uint32_t i=0;i<sample_len;i++) {
				temp32 = (*p_buf)*SOFT_GAIN;
				if(temp32>32767)
					*p_buf = 32767;
				else if(temp32<-32767)
					*p_buf = -32767;
				else
					*p_buf = temp32;
				p_buf++;
			}
			p_buf -= sample_len;

			data->type = SET_DATA_TYPE(SOUND,SOUND_MIC);
            send_data_to_stream(data);
		}
		else
		{
		 	_os_printf("%s:%d err\n",__FUNCTION__,__LINE__);

		}
	}
}

static int opcode_func(stream *s,void *priv,int opcode)
{
    static uint8_t *adc_audio_buf = NULL;
	int res = 0;
	switch(opcode)
	{
		case STREAM_OPEN_EXIT:
		{
			s->priv = (void*)os_malloc(sizeof(struct audio_adc_s));
			struct audio_adc_s *self_priv = (struct audio_adc_s*)s->priv;
			if(s->priv)
			{				
				self_priv->adc_msgq  = (void*)csi_kernel_msgq_new(1,sizeof(uint8_t*));
				OS_TASK_INIT("adc_audio_deal", &self_priv->thread_hdl, audio_deal_task, s, OS_TASK_PRIORITY_ABOVE_NORMAL, 1024);
			}
			uint32_t one_buf_size = (AUDIOLEN + MEAN_FILTER_SAMPLE_LEN*2);
            adc_audio_buf = ADUIO_MALLOC(AUDIONUM * one_buf_size);
			if(adc_audio_buf)
            {
			    stream_data_dis_mem_custom(s);
            }
			#ifdef PSRAM_HEAP
			//申请auadc的一个临时sram空间
			//临时空间的链表头,默认这里都是可以创建成功的
			void *queue_head = self_priv->queue = tqueue_init();
			//创建多个临时内存空间
			for(int i=0;i<AUDIOSRAM_NUM;i++)
			{
				void *mem_data = (void*)os_malloc(one_buf_size);
				struct tqueue_s *queue_data = tqueue_gen_data(mem_data,audio_custom_malloc);
				tqueue_push(queue_head,queue_data);
			}
			#endif
			//绑定到对应的流
            streamSrc_bind_streamDest(s,R_RECORD_AUDIO);
            streamSrc_bind_streamDest(s,R_RTP_AUDIO);
            streamSrc_bind_streamDest(s,R_AUDIO_TEST);
           	streamSrc_bind_streamDest(s,R_SPEAKER);
			streamSrc_bind_streamDest(s,R_AT_SAVE_AUDIO);
			streamSrc_bind_streamDest(s,R_AT_AVI_AUDIO);
			streamSrc_bind_streamDest(s,R_SPEECH_RECOGNITION);
			streamSrc_bind_streamDest(s,R_USB_AUDIO_MIC);
		}
		break;
		case STREAM_DATA_DIS:
		{
			struct data_structure *data = (struct data_structure *)priv;
			int data_num = (int)data->priv;
			data->type = DATA_TYPE_AUDIO_ADC;//设置声音的类型
            //data->priv = (void*)AUDIOLEN;
			set_stream_real_data_len(data,AUDIOLEN);
			//注册对应函数
			//data->ops = &stream_sound_ops;
			uint32_t one_buf_size = (AUDIOLEN + MEAN_FILTER_SAMPLE_LEN*2);
			data->data = adc_audio_buf + (data_num)*one_buf_size;
		}
		break;

		//音频发送之前先拷贝到psram
		case STREAM_SEND_DATA_START:
		{
			//在打开psram的情况下,才需要拷贝
			#ifdef PSRAM_HEAP
			struct data_structure *data = (struct data_structure *)priv;
			struct audio_adc_s *self_priv = (struct audio_adc_s*)s->priv;
			struct tqueue_s *queue_data = (struct tqueue_s *)data->priv;
			hw_memcpy(get_stream_real_data(data),queue_data->data,get_stream_real_data_len(data));
			//将priv返回队列中
			tqueue_push(self_priv->queue,queue_data);
			data->priv = NULL;
			#endif
		}
		break;

		default:
			//默认都返回成功
		break;
	}
	return res;
}

int audio_adc_start(void *audio_hdl)
{
	int ret = 0;
	int res = 0;
	void *buf;
	struct audio_ad_config *priv = (struct audio_ad_config*)audio_hdl;
	buf = priv->set_buf(priv->priv_el,&priv->current_node);
	if(!buf)
	{
		ret = -1;
		goto audio_adc_start_err;
	}
    priv->irq_func(priv , buf, priv->buf_size);
	audio_adc_start_err:
	return res;
}

static int32 global_audio_ad_read(struct audio_ad_config *audio, void* buf, uint32 len)
{
	auadc_read(audio->adc, buf, len);
	return 0;
}

//优先创建音频的流
stream *audio_adc_stream_init(const char *name)
{
	stream *s = open_stream_available(name,AUDIONUM,0,opcode_func,NULL);
	if(s)
	{
		global_audio_adc_s = s;
	}
	return s;
}

//关闭音频流
void audio_adc_stream_deinit()
{
	int res;
	if(global_audio_adc_s)
	{
		res = close_stream(global_audio_adc_s);
		if(!res)
		{
			global_audio_adc_s = NULL;
		}
	}
}

int audio_adc_init()
{	
	int res = 0;
    struct auadc_device *adc = (struct auadc_device *)dev_get(HG_AUADC_DEVID);

	stream *s = NULL;
	s = audio_adc_stream_init(S_ADC_AUDIO);
	if(!s)
	{
        res = -1;
		goto audio_adc_init_err;
	}
 
    struct audio_adc_s *audio_priv = (struct audio_adc_s*)s->priv;
	if(audio_priv)
	{
        struct audio_ad_config *ad_config = (struct audio_ad_config*)os_malloc(sizeof(struct audio_ad_config));
        memset(ad_config,0,sizeof(struct audio_ad_config));
        ad_config->adc = adc;
        ad_config->priv_el = s;
		audio_priv->audio_hardware_hdl = ad_config;
		audio_adc_register(ad_config,s,AUDIOLEN,audio_set_buf,audio_get_buf);
        ad_config->irq_func = global_audio_ad_read;
        auadc_open(adc, AUADC_SAMPLE_RATE_8K);
        auadc_request_irq(adc, AUADC_IRQ_FLAG_HALF | AUADC_IRQ_FLAG_FULL, (auadc_irq_hdl)audio_adc_irq, (uint32)ad_config);
        audio_adc_start(ad_config);
	}
	audio_adc_init_err:
	return res;
}

#ifdef PSRAM_HEAP
int audio_adc_deinit()
{
	struct auadc_device *adc = (struct auadc_device *)dev_get(HG_AUADC_DEVID);
	auadc_close(adc);
	stream *s = NULL;
	s = audio_adc_stream_init(S_ADC_AUDIO);
	//将对应资源释放管理好
	struct audio_adc_s *audio_priv = (struct audio_adc_s*)s->priv;
	struct audio_ad_config *ad_config = audio_priv->audio_hardware_hdl;
	os_printf("%s:%d\n",__FUNCTION__,__LINE__);
	//将adc保存的流数据节点释放(因为中断被关闭),这里正常可以将资源释放不至于异常
	if(ad_config->current_node)
	{
		os_printf("adc force current data:%X\n",ad_config->current_node);
		tqueue_push(audio_priv->queue,(struct tqueue_s *)ad_config->current_node);
		ad_config->current_node = NULL;
	}

	if(ad_config->reg_node)
	{
		os_printf("adc force reg_node data:%X\n",ad_config->reg_node);
		tqueue_push(audio_priv->queue,(struct tqueue_s *)ad_config->reg_node);
		ad_config->reg_node = NULL;
	}
	audio_adc_stream_deinit();
	return 0;
}
#else
int audio_adc_deinit()
{
	struct auadc_device *adc = (struct auadc_device *)dev_get(HG_AUADC_DEVID);
	auadc_close(adc);
	stream *s = NULL;
	s = audio_adc_stream_init(S_ADC_AUDIO);
	//将对应资源释放管理好
	struct audio_adc_s *audio_priv = (struct audio_adc_s*)s->priv;
	struct audio_ad_config *ad_config = audio_priv->audio_hardware_hdl;
	os_printf("%s:%d\n",__FUNCTION__,__LINE__);
	//将adc保存的流数据节点释放(因为中断被关闭),这里正常可以将资源释放不至于异常
	if(ad_config->current_node)
	{
		os_printf("adc force current data:%X\n",ad_config->current_node);
		force_del_data(ad_config->current_node);
		ad_config->current_node = NULL;
	}

	if(ad_config->reg_node)
	{
		os_printf("adc force reg_node data:%X\n",ad_config->reg_node);
		force_del_data(ad_config->reg_node);
		ad_config->reg_node = NULL;
	}
		
	audio_adc_stream_deinit();
	return 0;
}
#endif

//这个重新打开流(但流应该没有实际被重新打开,只是获取流的句柄,然后重新初始化adc)
int audio_adc_reinit()
{
	stream *s = NULL;
	struct auadc_device *adc = (struct auadc_device *)dev_get(HG_AUADC_DEVID);
	//这里应该流不能被释放过,所以s一定是要在之前就存在,否则后续的逻辑会有问题
	s = audio_adc_stream_init(S_ADC_AUDIO);
    struct audio_adc_s *audio_priv = (struct audio_adc_s*)s->priv;
	if(s && audio_priv)
	{
        struct audio_ad_config *ad_config = audio_priv->audio_hardware_hdl;
        memset(ad_config,0,sizeof(struct audio_ad_config));
        ad_config->adc = adc;
        ad_config->priv_el = s;
		audio_priv->audio_hardware_hdl = ad_config;
		audio_adc_register(ad_config,s,AUDIOLEN,audio_set_buf,audio_get_buf);
        ad_config->irq_func = global_audio_ad_read;
        auadc_open(adc, AUADC_SAMPLE_RATE_8K);
        auadc_request_irq(adc, AUADC_IRQ_FLAG_HALF | AUADC_IRQ_FLAG_FULL, (auadc_irq_hdl)audio_adc_irq, (uint32)ad_config);
        audio_adc_start(ad_config);
	}
	audio_adc_stream_deinit();
	return 0;
}


