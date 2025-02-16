//////////////////////////////////////////////////////////////////////////////////
// nvme_admin_cmd.c for Cosmos+ OpenSSD
// Copyright (c) 2016 Hanyang University ENC Lab.
// Contributed by Yong Ho Song <yhsong@enc.hanyang.ac.kr>
//				  Youngjin Jo <yjjo@enc.hanyang.ac.kr>
//				  Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//
// This file is part of Cosmos+ OpenSSD.
//
// Cosmos+ OpenSSD is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3, or (at your option)
// any later version.
//
// Cosmos+ OpenSSD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Cosmos+ OpenSSD; see the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Company: ENC Lab. <http://enc.hanyang.ac.kr>
// Engineer: Sangjin Lee <sjlee@enc.hanyang.ac.kr>
//
// Project Name: Cosmos+ OpenSSD
// Design Name: Cosmos+ Firmware
// Module Name: NVMe Admin Command Handler
// File Name: nvme_admin_cmd.c
//
// Version: v1.0.0
//
// Description:
//   - handles NVMe admin command
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
// Revision History:
//
// * v1.0.0
//   - First draft
//////////////////////////////////////////////////////////////////////////////////


#include "xil_printf.h"
#include "xtime_l.h"
#include "debug.h"
#include "string.h"
#include "io_access.h"

#include "nvme.h"
#include "host_lld.h"
#include "nvme_identify.h"
#include "nvme_admin_cmd.h"
#include "../page_map.h"
#include "../memory_map.h"
#include "../low_level_scheduler.h"
#include "../search.h"
#include "../hardware_accelerate.h"

extern NVME_CONTEXT g_nvmeTask;

unsigned int get_num_of_queue(unsigned int dword11)
{
	ADMIN_SET_FEATURES_NUMBER_OF_QUEUES_DW11 numOfQueue;

	xil_printf("num_of_queue %X\r\n", dword11);

	numOfQueue.dword = dword11;

	if(numOfQueue.NSQR >= MAX_NUM_OF_IO_SQ)
		numOfQueue.NSQR = MAX_NUM_OF_IO_SQ - 1;

	if(numOfQueue.NCQR >= MAX_NUM_OF_IO_CQ)
		numOfQueue.NCQR = MAX_NUM_OF_IO_CQ - 1;

	return numOfQueue.dword;
}

void handle_set_features(NVME_ADMIN_COMMAND *nvmeAdminCmd, NVME_COMPLETION *nvmeCPL, unsigned int* printIOinfo)
{
	ADMIN_SET_FEATURES_DW10 features;

	features.dword = nvmeAdminCmd->dword10;

	switch(features.FID)
	{
		case NUMBER_OF_QUEUES:
		{
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = get_num_of_queue(nvmeAdminCmd->dword11);
			break;
		}
		case INTERRUPT_COALESCING:
		{
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = 0x0;
			break;
		}
		case ARBITRATION:
		{
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = 0x0;
			break;
		}
		case ASYNCHRONOUS_EVENT_CONFIGURATION:
		{
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = 0x0;
			break;
		}
		case VOLATILE_WRITE_CACHE:
		{
			xil_printf("Set VWC: %X\r\n", nvmeAdminCmd->dword11);
			g_nvmeTask.cacheEn = (nvmeAdminCmd->dword11 & 0x1);
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = 0x0;
			break;
		}
		case POWER_MANAGEMENT:
		{
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = 0x0;
			break;
		}
		case 0x11:  // own case, to change the value of print_IO_info
		{
			if(nvmeAdminCmd->dword12 == 0){
				*printIOinfo = 0;
				xil_printf("print IO info has disabled!\r\n");
			}
			else{
				*printIOinfo = 1;
				xil_printf("print IO info has enabled!\r\n");
			}
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = 0x0;
			break;
		}
		default:
		{
			xil_printf("Not Support FID (Set): %X\r\n", features.FID);
			ASSERT(0);
			break;
		}
	}
	xil_printf("Set Feature FID:%X\r\n", features.FID);
}

int handle_get_features(NVME_ADMIN_COMMAND *nvmeAdminCmd, NVME_COMPLETION *nvmeCPL, unsigned int cmdSlotTag)
{
	ADMIN_GET_FEATURES_DW10 features;
	NVME_COMPLETION cpl;

	features.dword = nvmeAdminCmd->dword10;

	switch(features.FID)
	{
		case LBA_RANGE_TYPE:
		{
			ASSERT(nvmeAdminCmd->NSID == 1);

			cpl.dword[0] = 0x0;
			cpl.statusField.SC = INVALID_FIELD_IN_COMMAND;
			nvmeCPL->dword[0] = cpl.dword[0];
			nvmeCPL->specific = 0x0;
			break;
		}
		case TEMPERATURE_THRESHOLD:
		{
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = nvmeAdminCmd->dword11;
			break;
		}
		case VOLATILE_WRITE_CACHE:
		{
			
			xil_printf("Get VWC: %X\r\n", g_nvmeTask.cacheEn);
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = g_nvmeTask.cacheEn;
			break;
		}
		case POWER_MANAGEMENT:
		{
			nvmeCPL->dword[0] = 0x0;
			nvmeCPL->specific = 0x0;
			break;
		}
		case 0x11:  // not need retrieve
		{
			set_auto_rx_dma(cmdSlotTag, 0, DMA_TASK_CONFIG_ADDR);
			searchTask->taskValid = 1;
			searchTask->cmdSlotTag = cmdSlotTag;
			searchTask->pageCompleteCount = 0;
			searchTask->totalHitCounts = 0;
			searchTask->searchPageNum = 0;
			searchTask->rxDmaExe = 1;
			searchTask->rxDmaTail = g_hostDmaStatus.fifoTail.autoDmaRx;
			searchTask->rxDmaOverFlowCnt = g_hostDmaAssistStatus.autoDmaRxOverFlowCnt;
			searchTask->need_path_walk = 0;
			reservedReq = 1;  

			// set_auto_nvme_cpl(cmdSlotTag, 0x0, 0x0);
			// break;
			return 1;
		}
		case 0x12:  // need retrieve
		{
			XTime_GetTime(&time_start_search);
			set_auto_rx_dma(cmdSlotTag, 0, DMA_TASK_CONFIG_ADDR); //鎺ュ彈鍏抽敭瀛�
			searchTask->taskValid = 1;
			searchTask->cmdSlotTag = cmdSlotTag;
			searchTask->pageCompleteCount = 0;
			searchTask->totalHitCounts = 0;
			searchTask->searchPageNum = 0;
			searchTask->rxDmaExe = 1; //need execute DMA
			searchTask->rxDmaTail = g_hostDmaStatus.fifoTail.autoDmaRx;
			searchTask->rxDmaOverFlowCnt = g_hostDmaAssistStatus.autoDmaRxOverFlowCnt;
			searchTask->need_path_walk = 1;
			reservedReq = 1;

			// set_auto_nvme_cpl(cmdSlotTag, 0x0, 0x0);
			// break;
			return 1;
		}
		case GET_INTEGRATE_BLOCK_ADDR: //澶勭悊鑱氬悎鐨勫潡鍦板潃
		{
			XTime start_dma, end_send, end_receive;
			XTime_GetTime(&start_dma);
			set_auto_rx_dma(cmdSlotTag, 0, INTEGRATE_BLOCK_ADDR); //鎺ュ彈鍏抽敭瀛�
			set_auto_rx_dma(cmdSlotTag, 1, INTEGRATE_BLOCK_ADDR+4096); //鎺ュ彈鍏抽敭瀛�
			XTime_GetTime(&end_send);
			unsigned long loop_time=0;
			while(!check_auto_rx_dma_partial_done(g_hostDmaStatus.fifoTail.autoDmaRx, g_hostDmaAssistStatus.autoDmaRxOverFlowCnt)){
				loop_time++;
			}
			XTime_GetTime(&end_receive);
			char* dma_data = (char*)INTEGRATE_BLOCK_ADDR;
			char* dma_data_4k = (char*)(INTEGRATE_BLOCK_ADDR+4096);
			printf("DMA_receive:%s\r\n",dma_data);
			printf("DMA_receive at 4k:%s\r\n",dma_data_4k);

			unsigned int dma_auto, dma_done;
			dma_auto = ((end_send - start_dma) * 1000000) / (COUNTS_PER_SECOND);
			dma_done = ((end_receive - end_send) * 1000000) / (COUNTS_PER_SECOND);
			printf("dma_auto spend:%lu us\r\n",dma_auto);
			printf("check_dma done spend:%lu us, loop %lu times\r\n",dma_done, loop_time);
			break;
		}
		// case 0x13:  // test crawler
		// {
		// 	unsigned int dev_addr = FS_FOPEN_PATH_ADDR;
		// 	unsigned int path_len = 0;
		// 	set_auto_rx_dma(cmdSlotTag, 0, FS_FOPEN_PATH_ADDR);

		// 	path_len = *((unsigned int *)dev_addr);
		// 	if (path_len == 0){
		// 		xil_printf("path_len == 0 !!!\r\n");
		// 		set_auto_nvme_cpl(cmdSlotTag, 0x0, 0x0);
		// 		return 1;				
		// 	}

		// 	char path[path_len + 1];
		// 	memcpy(path, dev_addr, path_len);
		// 	path[path_len] = '\0';

		// 	unsigned int ino = f2fs_path(path, path_len);
		// 	read_inode(ino);

		// 	set_auto_nvme_cpl(cmdSlotTag, 0x0, 0x0);
		// 	return 1;
		// }
//		case 0x14:  // own case, to measure flash's bandwidth
//		{
//			EmptyReqQ();
//			XTime tEnd, tCur;
//			unsigned int tUsed, i;
//			LOW_LEVEL_REQ_INFO lowLevelCmd;
//			unsigned int tempLpn = nvmeAdminCmd->dword12;  // start lpn
//
//			for(i=0;i<1024;i++){  // fill the reqQueue
//				unsigned int dieNo = tempLpn % DIE_NUM;
//				lowLevelCmd.rowAddr = pageMap->pmEntry[dieNo][tempLpn/DIE_NUM].ppn;
//				lowLevelCmd.spareDataBuf = SPARE_ADDR;
//				// lowLevelCmd.bufferEntry = 0;
//				lowLevelCmd.chNo = dieNo % CHANNEL_NUM;
//				lowLevelCmd.wayNo = dieNo / CHANNEL_NUM;
//				lowLevelCmd.request = V2FCommand_ReadPageTrigger;
//				// lowLevelCmd.search = 0;
//				PushToReqQueue(&lowLevelCmd);
//				tempLpn++;
//			}
//			tempLpn = nvmeAdminCmd->dword12;  // reset startLpn
//			XTime_GetTime(&tCur);
//			for(i=0; i<nvmeAdminCmd->dword13; i++){
//				unsigned int dieNo = tempLpn % DIE_NUM;
//				lowLevelCmd.rowAddr = pageMap->pmEntry[dieNo][tempLpn/DIE_NUM].ppn;
//				lowLevelCmd.spareDataBuf = SPARE_ADDR;
//				// lowLevelCmd.bufferEntry = 0;
//				lowLevelCmd.chNo = dieNo % CHANNEL_NUM;
//				lowLevelCmd.wayNo = dieNo / CHANNEL_NUM;
//				lowLevelCmd.request = V2FCommand_ReadPageTrigger;
//				// lowLevelCmd.search = 0;
//				PushToReqQueue(&lowLevelCmd);
//				tempLpn++;
//			}
//			XTime_GetTime(&tEnd);
//			tUsed = ((tEnd-tCur)*1000000)/(COUNTS_PER_SECOND);
//			xil_printf("%d pages done, %d us used.\r\n", nvmeAdminCmd->dword13, tUsed);
//
//			set_auto_nvme_cpl(cmdSlotTag, 0x0, 0x0);
//			break;
//		}
		default:
		{
			xil_printf("Not Support FID (Get): %X\r\n", features.FID);
			ASSERT(0);
			break;
		}
	}
	xil_printf("Get Feature FID:%X\r\n", features.FID);
	return 0;
}

void handle_create_io_sq(NVME_ADMIN_COMMAND *nvmeAdminCmd, NVME_COMPLETION *nvmeCPL)
{
	ADMIN_CREATE_IO_SQ_DW10 sqInfo10;
	ADMIN_CREATE_IO_SQ_DW11 sqInfo11;
	NVME_IO_SQ_STATUS *ioSqStatus;
	unsigned int ioSqIdx;

	sqInfo10.dword = nvmeAdminCmd->dword10;
	sqInfo11.dword = nvmeAdminCmd->dword11;

	xil_printf("create sq: 0x%08X, 0x%08X\r\n", sqInfo11.dword, sqInfo10.dword);

	ASSERT((nvmeAdminCmd->PRP1[0] & 0xF) == 0 && nvmeAdminCmd->PRP1[1] < 0x10);
	ASSERT(0 < sqInfo10.QID && sqInfo10.QID <= 8 && sqInfo10.QSIZE < 0x100 && 0 < sqInfo11.CQID && sqInfo11.CQID <= 8);

	ioSqIdx = sqInfo10.QID - 1;
	ioSqStatus = g_nvmeTask.ioSqInfo + ioSqIdx;

	ioSqStatus->valid = 1;
	ioSqStatus->qSzie = sqInfo10.QSIZE;
	ioSqStatus->cqVector = sqInfo11.CQID;
	ioSqStatus->pcieBaseAddrL = nvmeAdminCmd->PRP1[0];
	ioSqStatus->pcieBaseAddrH = nvmeAdminCmd->PRP1[1];

	set_io_sq(ioSqIdx, ioSqStatus->valid, ioSqStatus->cqVector, ioSqStatus->qSzie, ioSqStatus->pcieBaseAddrL, ioSqStatus->pcieBaseAddrH);

	nvmeCPL->dword[0] = 0;
	nvmeCPL->specific = 0x0;

}

void handle_delete_io_sq(NVME_ADMIN_COMMAND *nvmeAdminCmd, NVME_COMPLETION *nvmeCPL)
{
	ADMIN_DELETE_IO_SQ_DW10 sqInfo10;
	NVME_IO_SQ_STATUS *ioSqStatus;
	unsigned int ioSqIdx;

	sqInfo10.dword = nvmeAdminCmd->dword10;

	xil_printf("delete sq: 0x%08X\r\n", sqInfo10.dword);

	ioSqIdx = (unsigned int)sqInfo10.QID - 1;
	ioSqStatus = g_nvmeTask.ioSqInfo + ioSqIdx;

	ioSqStatus->valid = 0;
	ioSqStatus->cqVector = 0;
	ioSqStatus->qSzie = 0;
	ioSqStatus->pcieBaseAddrL = 0;
	ioSqStatus->pcieBaseAddrH = 0;

	set_io_sq(ioSqIdx, 0, 0, 0, 0, 0);

	nvmeCPL->dword[0] = 0;
	nvmeCPL->specific = 0x0;
}


void handle_create_io_cq(NVME_ADMIN_COMMAND *nvmeAdminCmd, NVME_COMPLETION *nvmeCPL)
{
	ADMIN_CREATE_IO_CQ_DW10 cqInfo10;
	ADMIN_CREATE_IO_CQ_DW11 cqInfo11;
	NVME_IO_CQ_STATUS *ioCqStatus;
	unsigned int ioCqIdx;

	cqInfo10.dword = nvmeAdminCmd->dword10;
	cqInfo11.dword = nvmeAdminCmd->dword11;

	xil_printf("create cq: 0x%08X, 0x%08X\r\n", cqInfo11.dword, cqInfo10.dword);

	ASSERT(((nvmeAdminCmd->PRP1[0] & 0xF) == 0) && (nvmeAdminCmd->PRP1[1] < 0x10));
	ASSERT(cqInfo11.IV < 8 && cqInfo10.QSIZE < 0x100 && 0 < cqInfo10.QID && cqInfo10.QID <= 8);

	ioCqIdx = cqInfo10.QID - 1;
	ioCqStatus = g_nvmeTask.ioCqInfo + ioCqIdx;

	ioCqStatus->valid = 1;
	ioCqStatus->qSzie = cqInfo10.QSIZE;
	ioCqStatus->irqEn = cqInfo11.IEN;
	ioCqStatus->irqVector = cqInfo11.IV;
	ioCqStatus->pcieBaseAddrL = nvmeAdminCmd->PRP1[0];
	ioCqStatus->pcieBaseAddrH = nvmeAdminCmd->PRP1[1];

	set_io_cq(ioCqIdx, ioCqStatus->valid, ioCqStatus->irqEn, ioCqStatus->irqVector, ioCqStatus->qSzie, ioCqStatus->pcieBaseAddrL, ioCqStatus->pcieBaseAddrH);

	nvmeCPL->dword[0] = 0;
	nvmeCPL->specific = 0x0;
}

void handle_delete_io_cq(NVME_ADMIN_COMMAND *nvmeAdminCmd, NVME_COMPLETION *nvmeCPL)
{
	ADMIN_DELETE_IO_CQ_DW10 cqInfo10;
	NVME_IO_CQ_STATUS *ioCqStatus;
	unsigned int ioCqIdx;

	cqInfo10.dword = nvmeAdminCmd->dword10;

	xil_printf("delete cq: 0x%08X\r\n", cqInfo10.dword);

	ioCqIdx = (unsigned int)cqInfo10.QID - 1;
	ioCqStatus = g_nvmeTask.ioCqInfo + ioCqIdx;

	ioCqStatus->valid = 0;
	ioCqStatus->irqVector = 0;
	ioCqStatus->qSzie = 0;
	ioCqStatus->pcieBaseAddrL = 0;
	ioCqStatus->pcieBaseAddrH = 0;
	
	set_io_cq(ioCqIdx, 0, 0, 0, 0, 0, 0);

	nvmeCPL->dword[0] = 0;
	nvmeCPL->specific = 0x0;
}

void handle_identify(NVME_ADMIN_COMMAND *nvmeAdminCmd, NVME_COMPLETION *nvmeCPL)
{
	ADMIN_IDENTIFY_COMMAND_DW10 identifyInfo;
	unsigned int pIdentifyData = ADMIN_CMD_DRAM_DATA_BUFFER;
	unsigned int prp[2];
	unsigned int prpLen;

	identifyInfo.dword = nvmeAdminCmd->dword10;

	if(identifyInfo.CNS == 1)
	{
		if((nvmeAdminCmd->PRP1[0] & 0xF) != 0 || (nvmeAdminCmd->PRP2[0] & 0xF) != 0)
			xil_printf("CI: %X, %X, %X, %X\r\n", nvmeAdminCmd->PRP1[1], nvmeAdminCmd->PRP1[0], nvmeAdminCmd->PRP2[1], nvmeAdminCmd->PRP2[0]);

		ASSERT((nvmeAdminCmd->PRP1[0] & 0xF) == 0 && (nvmeAdminCmd->PRP2[0] & 0xF) == 0);
		identify_controller(pIdentifyData);
	}
	else if(identifyInfo.CNS == 0)
	{
		if((nvmeAdminCmd->PRP1[0] & 0xF) != 0 || (nvmeAdminCmd->PRP2[0] & 0xF) != 0)
			xil_printf("NI: %X, %X, %X, %X\r\n", nvmeAdminCmd->PRP1[1], nvmeAdminCmd->PRP1[0], nvmeAdminCmd->PRP2[1], nvmeAdminCmd->PRP2[0]);

		//ASSERT(nvmeAdminCmd->NSID == 1);
		ASSERT((nvmeAdminCmd->PRP1[0] & 0xF) == 0 && (nvmeAdminCmd->PRP2[0] & 0xF) == 0);
		identify_namespace(pIdentifyData);
	}
	else
		ASSERT(0);
	
	prp[0] = nvmeAdminCmd->PRP1[0];
	prp[1] = nvmeAdminCmd->PRP1[1];

	prpLen = 0x1000 - (prp[0] & 0xFFF);

	set_direct_tx_dma(pIdentifyData, prp[1], prp[0], prpLen);

	if(prpLen != 0x1000)
	{
		pIdentifyData = pIdentifyData + prpLen;
		prpLen = 0x1000 - prpLen;
		prp[0] = nvmeAdminCmd->PRP2[0];
		prp[1] = nvmeAdminCmd->PRP2[1];

		ASSERT((prp[1] & 0xFFF) == 0);

		set_direct_tx_dma(pIdentifyData, prp[1], prp[0], prpLen);
	}

	check_direct_tx_dma_done();

	nvmeCPL->dword[0] = 0;
	nvmeCPL->specific = 0x0;
}

void handle_get_log_page(NVME_ADMIN_COMMAND *nvmeAdminCmd, NVME_COMPLETION *nvmeCPL)
{
	//ADMIN_GET_LOG_PAGE_DW10 getLogPageInfo;

	//unsigned int prp1[2];
	//unsigned int prp2[2];
	//unsigned int prpLen;

	//getLogPageInfo.dword = nvmeAdminCmd->dword10;

	//prp1[0] = nvmeAdminCmd->PRP1[0];
	//prp1[1] = nvmeAdminCmd->PRP1[1];
	//prpLen = 0x1000 - (prp1[0] & 0xFFF);

	//prp2[0] = nvmeAdminCmd->PRP2[0];
	//prp2[1] = nvmeAdminCmd->PRP2[1];

	//xil_printf("ADMIN GET LOG PAGE\n");

	//LID
	//Mandatory//1-Error information, 2-SMART/Health information, 3-Firmware Slot information
	//Optional//4-ChangedNamespaceList, 5-Command Effects Log
	//xil_printf("LID: 0x%X, NUMD: 0x%X \r\n", getLogPageInfo.LID, getLogPageInfo.NUMD);

	//xil_printf("PRP1[63:32] = 0x%X, PRP1[31:0] = 0x%X", prp1[1], prp1[0]);
	//xil_printf("PRP2[63:32] = 0x%X, PRP2[31:0] = 0x%X", prp2[1], prp2[0]);

	nvmeCPL->dword[0] = 0;
	nvmeCPL->specific = 0x9;//invalid log page
}

void handle_nvme_admin_cmd(NVME_COMMAND *nvmeCmd, unsigned int* printIOinfo)
{
	NVME_ADMIN_COMMAND *nvmeAdminCmd;
	NVME_COMPLETION nvmeCPL;
	unsigned int opc;
	unsigned int needCpl;
	unsigned int needSlotRelease;

	nvmeAdminCmd = (NVME_ADMIN_COMMAND*)nvmeCmd->cmdDword;
	opc = (unsigned int)nvmeAdminCmd->OPC;


	needCpl = 1;
	needSlotRelease = 0;
	switch(opc)
	{
		case ADMIN_SET_FEATURES:
		{
			handle_set_features(nvmeAdminCmd, &nvmeCPL, printIOinfo);
			break;
		}
		case ADMIN_CREATE_IO_CQ:
		{
			handle_create_io_cq(nvmeAdminCmd, &nvmeCPL);
			break;
		}
		case ADMIN_CREATE_IO_SQ:
		{
			handle_create_io_sq(nvmeAdminCmd, &nvmeCPL);
			break;
		}
		case ADMIN_IDENTIFY:
		{
			handle_identify(nvmeAdminCmd, &nvmeCPL);
			break;
		}
		case ADMIN_GET_FEATURES:
		{
			if (handle_get_features(nvmeAdminCmd, &nvmeCPL, nvmeCmd->cmdSlotTag))
				return;
			break;
		}
		case ADMIN_DELETE_IO_CQ:
		{
			handle_delete_io_cq(nvmeAdminCmd, &nvmeCPL);
			break;
		}
		case ADMIN_DELETE_IO_SQ:
		{
			handle_delete_io_sq(nvmeAdminCmd, &nvmeCPL);
			break;
		}
		case ADMIN_ASYNCHRONOUS_EVENT_REQUEST:
		{
			needCpl = 0;
			needSlotRelease = 1;
			nvmeCPL.dword[0] = 0;
			nvmeCPL.specific = 0x0;
			break;
		}
		case ADMIN_GET_LOG_PAGE:
		{
			handle_get_log_page(nvmeAdminCmd, &nvmeCPL);
			break;
		}

		default:
		{
			xil_printf("Not Support Admin Command OPC: %X\r\n", opc);
			//ASSERT(0);
			break;
		}
	}

	if(needCpl == 1)
		set_auto_nvme_cpl(nvmeCmd->cmdSlotTag, nvmeCPL.specific, nvmeCPL.statusFieldWord);
	else if(needSlotRelease == 1)
		set_nvme_slot_release(nvmeCmd->cmdSlotTag);
	else
		set_nvme_cpl(0, 0, nvmeCPL.specific, nvmeCPL.statusFieldWord);

	// xil_printf("Done Admin Command OPC: %X\r\n", opc);

}

