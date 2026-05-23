
#include <basic_include.h>
#include <lwip/netif.h>

#if LWIP_PCAP
/*
* pcap file format
* ---------------+---------------+---------------
* |PCAP Header   | Packet header | Packet data  |
* |Packet header | Packet data   | Packet data  |
*------------------------------------------------
*/

#define PCAP_FILE_LINK_TYPE_BSD                 0
#define PCAP_FILE_LINK_TYPE_ETHERNET            1
#define PCAP_FILE_LINK_TYPE_802D5               6
#define PCAP_FILE_LINK_TYPE_ARCNET              7
#define PCAP_FILE_LINK_TYPE_SLIP                8
#define PCAP_FILE_LINK_TYPE_PPP                 9
#define PCAP_FILE_LINK_TYPE_FDDI                10
#define PCAP_FILE_LINK_TYPE_LLC_SNAPATM         100
#define PCAP_FILE_LINK_TYPE_RAW_IP              101
#define PCAP_FILE_LINK_TYPE_BSD_OSSLIP          102
#define PCAP_FILE_LINK_TYPE_BSD_OSPPP           103
#define PCAP_FILE_LINK_TYPE_CISCOHDLC           104
#define PCAP_FILE_LINK_TYPE_802D11              105
#define PCAP_FILE_LINK_TYPE_OPENBSD             108
#define PCAP_FILE_LINK_TYPE_LINUXCOOKED         113
#define PCAP_FILE_LINK_TYPE_LOCALTALK           114

#define FILE void

//PACP Header
struct pcap_hdr {
    uint32 magic_number;
    uint16 version_major;
    uint16 version_minor;
    uint32 thiszone;
    uint32 sigfigs;
    uint32 snaplen;
    uint32 linktype;
};

//Packet Header
struct pcap_pkt_hdr {
    uint32 ts_sec;
    uint32 ts_usec;
    uint32 incl_len;
    uint32 orig_len;
};

struct pcap {
    struct netif         *netif;
    struct pcap_hdr       pcap_hdr;
    struct pcap_pkt_hdr   pkt_hdr;
    FILE                 *fp;
    char                  pcap_file[128];
    uint32                write: 1, stop: 1;
};

int fclose(FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
FILE *fopen(const char *filename, const char *mode);
int fsync(FILE *fd);

static inline int link_type(struct netif *netif)
{
    int type = PCAP_FILE_LINK_TYPE_ETHERNET;

    if (netif->name[0] == 'p' && netif->name[1] == 'p') {
        type = PCAP_FILE_LINK_TYPE_PPP;
    } else if (netif->name[0] == 'e' && netif->name[1] == 'n') {
        type = PCAP_FILE_LINK_TYPE_ETHERNET;
    } else if (netif->name[0] == 'l' && netif->name[1] == 'o') {
        type = PCAP_FILE_LINK_TYPE_ETHERNET;
    }

    return type;
}

static void pcap_free(struct pcap *pcap)
{
    if (pcap->fp) {
        fclose(pcap->fp);
    }
    os_free(pcap);
}

static struct pcap *pcap_new(struct netif *netif)
{
    struct pcap *pcap = (struct pcap *)os_malloc(sizeof(struct pcap));
    if (pcap) {
        os_memset(pcap, 0, sizeof(struct pcap));
        pcap->netif = netif;
    }
    return pcap;
}

int pcap_open(struct pcap *pcap, char *file_path)
{
    int ret = -1;
    FILE *fp = NULL;

    if (pcap == NULL || file_path == NULL) {
        return -1;
    }

    fp = fopen(file_path, "w+");
    if (fp) {
        os_memset(&pcap->pcap_hdr, 0, sizeof(struct pcap_hdr));
        os_memset(&pcap->pkt_hdr, 0, sizeof(struct pcap_pkt_hdr));
        pcap->pcap_hdr.magic_number = 0xa1b2c3d4;
        pcap->pcap_hdr.version_major = 2;
        pcap->pcap_hdr.version_minor = 4;
        pcap->pcap_hdr.thiszone = 0;
        pcap->pcap_hdr.sigfigs = 0;
        pcap->pcap_hdr.snaplen = 65535;
        pcap->pcap_hdr.linktype = 1;//link_type(pcap->netif);

        if (fwrite(&pcap->pcap_hdr, sizeof(struct pcap_hdr), 1, fp) > 0) {
            pcap->fp = fp;
            fsync(fp);
            ret = 0;
        }

        os_printf("pcap open:%s OK\n", pcap->pcap_file);
    } else {
        os_printf("pcap open %s fail\r\n", pcap->pcap_file);
    }

    return ret;
}

int pcap_write(struct pcap *pcap, char *data, int len)
{
    uint64 ticks = 0;
    int ret1 = -1, ret2 = -1;

    ticks = os_jiffies();
    if (pcap->fp) {
        pcap->pkt_hdr.ts_sec   = os_jiffies_to_msecs(ticks) / 1000;
        pcap->pkt_hdr.ts_usec  = (os_jiffies_to_msecs(ticks) % 1000) * 1000;
        pcap->pkt_hdr.incl_len = len;
        pcap->pkt_hdr.orig_len = len;
        ret1 = fwrite(&pcap->pkt_hdr, sizeof(struct pcap_pkt_hdr), 1, pcap->fp);
        ret2 = fwrite(data, 1, len, pcap->fp);

        if (ret1 < 0 || ret2 < 0) {
            fclose(pcap->fp);
            pcap->fp = NULL;
        } else {
            fsync(pcap->fp);
        }
    }

    return 1;
}

int pcap_write_scatter(struct pcap *pcap, scatter_data *scat_data, int count)
{
    int i;
    int ret1;
    int ret2 = 0;
    int len = 0;
    uint64 ticks = 0;

    for (i = 0; i < count; i++) 
        len += scat_data[i].size;

    ticks = os_jiffies();
    if (pcap->fp) {
        pcap->pkt_hdr.ts_sec   = os_jiffies_to_msecs(ticks) / 1000;
        pcap->pkt_hdr.ts_usec  = (os_jiffies_to_msecs(ticks) % 1000) * 1000;
        pcap->pkt_hdr.incl_len = len;
        pcap->pkt_hdr.orig_len = len;
        ret1 = fwrite(&pcap->pkt_hdr, sizeof(struct pcap_pkt_hdr), 1, pcap->fp);
        for (i = 0; i < count && ret2 >= 0; i++) {
            ret2 = fwrite(scat_data[i].addr, 1, scat_data[i].size, pcap->fp);
        }

        if (ret1 < 0 || ret2 < 0) {
            fclose(pcap->fp);
            pcap->fp = NULL;
        } else {
            fsync(pcap->fp);
        }
    }

    return 1;
}

void pcap_stop(struct netif *netif)
{
    struct pcap *p = netif->pcap;
    if(p){
        uint32 flags = disable_irq();
        p->stop = 1;
        enable_irq(flags);
        while (p->write) os_sleep_ms(1);
        pcap_free(netif->pcap);
        netif->pcap = NULL;
    }
}

char *pcap_start(struct netif *netif, char *save_dir)
{
    struct pcap *pcap = NULL;

    if (netif == NULL || save_dir == NULL) {
        return NULL;
    }

    if (netif->pcap) {
        os_printf("netif busy!\r\n");
        return NULL;
    }

    pcap = pcap_new(netif);
    if (pcap) {
        sprintf(pcap->pcap_file, "%s/%c%c-%llu.pcap", save_dir, netif->name[0], netif->name[1], os_jiffies());
        if (pcap_open(pcap, pcap->pcap_file)) {
            pcap_free(pcap);
            pcap = NULL;
        } else {
            os_printf("pcap file: %s\n", pcap->pcap_file);
        }
    }

    netif->pcap = pcap;
    return (pcap ? pcap->pcap_file : NULL);
}

void pcap(struct netif *netif, char *data, int len)
{
    struct pcap *p = netif->pcap;
    uint32 flags = disable_irq();
    if (p && !p->stop) {
        p->write = 1;
    }
    enable_irq(flags);
    if (p->write) {
        pcap_write(netif->pcap, data, len);
    }
    p->write = 0;
}

void pcap_scatter(struct netif *netif, scatter_data *scat_data, int count)
{
    struct pcap *p = netif->pcap;
    uint32 flags = disable_irq();
    if (p && !p->stop) {
        p->write = 1;
    }
    enable_irq(flags);
    if (p->write) {
        pcap_write_scatter(netif->pcap, scat_data, count);
    }
    p->write = 0;
}
#endif
