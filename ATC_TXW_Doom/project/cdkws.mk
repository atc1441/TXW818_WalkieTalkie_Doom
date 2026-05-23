.PHONY: clean All Project_Title Project_PreBuild Project_Build Project_PostBuild

All: Project_Title Project_PreBuild Project_Build Project_PostBuild

Project_Title:
	@echo "----------Building project:[ ATC_TXW_Doom - FLASH ]----------"

Project_PreBuild:
	@echo Executing Pre Build commands ...
	@export CDKPath="D:/C-Sky/CDK" CDK_VERSION="V2.24.5" CPU="CK803S" ProjectName="ATC_TXW_Doom" ProjectPath="C:/Users/pc/Documents/GitHub/TXW818_WalkieTalkie_Doom/ATC_TXW_Doom/project/" && C:/Users/pc/Documents/GitHub/TXW818_WalkieTalkie_Doom/ATC_TXW_Doom/project/prebuild.sh $<
	@echo Done

Project_Build:
	@make -r -f ATC_TXW_Doom.mk -j 24 -C  ./ 

Project_PostBuild:
	@echo Executing Post Build commands ...
	@export CDKPath="D:/C-Sky/CDK" CDK_VERSION="V2.24.5" CPU="CK803S" ProjectName="ATC_TXW_Doom" ProjectPath="C:/Users/pc/Documents/GitHub/TXW818_WalkieTalkie_Doom/ATC_TXW_Doom/project/" && C:/Users/pc/Documents/GitHub/TXW818_WalkieTalkie_Doom/ATC_TXW_Doom/project/BuildBIN.sh
	@echo Done


clean:
	@echo "----------Cleaning project:[ ATC_TXW_Doom - FLASH ]----------"

