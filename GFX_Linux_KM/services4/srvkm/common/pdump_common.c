/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 * 
 * This program is distributed in the hope it will be useful but, except 
 * as otherwise stated in writing, without any warranty; without even the 
 * implied warranty of merchantability or fitness for a particular purpose. 
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK 
 *
 ******************************************************************************/

#if defined(PDUMP)
#include <stdarg.h>

#include "services_headers.h"
#if defined(SUPPORT_SGX)
#include "sgxdefs.h"
#include "sgxmmu.h"
#endif 
#include "pdump_km.h"

#if !defined(PDUMP_TEMP_BUFFER_SIZE)
#define PDUMP_TEMP_BUFFER_SIZE (64 * 1024L)
#endif

#if 1
#define PDUMP_DBG(a)   PDumpOSDebugPrintf a
#else
#define PDUMP_DBG(a)
#endif

#define PDUMP_DATAMASTER_PIXEL		(1)
#define PDUMP_DATAMASTER_EDM		(3)

#define	MIN(x, y) (((x) < (y)) ? (x) : (y))
#define	PTR_PLUS(t, p, x) ((t *)(((IMG_CHAR *)(p)) + (x)))
#define	VPTR_PLUS(p, x) PTR_PLUS(IMG_VOID, p, x)
#define	VPTR_INC(p, x) (p = VPTR_PLUS(p, x))
#define MAX_PDUMP_MMU_CONTEXTS	(32)
static IMG_VOID *gpvTempBuffer = IMG_NULL;
static IMG_HANDLE ghTempBufferBlockAlloc;
static IMG_UINT16 gui16MMUContextUsage = 0;



static IMG_VOID *GetTempBuffer(IMG_VOID)
{
	
	if (gpvTempBuffer == IMG_NULL)
	{
		PVRSRV_ERROR eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
					  PDUMP_TEMP_BUFFER_SIZE,
					  &gpvTempBuffer,
					  &ghTempBufferBlockAlloc,
					  "PDUMP Temporary Buffer");
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "GetTempBuffer: OSAllocMem failed: %d", eError));
		}
	}

	return gpvTempBuffer;
}

static IMG_VOID FreeTempBuffer(IMG_VOID)
{

	if (gpvTempBuffer != IMG_NULL)
	{
		PVRSRV_ERROR eError = OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
					  PDUMP_TEMP_BUFFER_SIZE,
					  gpvTempBuffer,
					  ghTempBufferBlockAlloc);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "FreeTempBuffer: OSFreeMem failed: %d", eError));
		}
		else
		{
			gpvTempBuffer = IMG_NULL;
		}
	}
}

IMG_VOID PDumpInitCommon(IMG_VOID)
{
	
	(IMG_VOID) GetTempBuffer();

	
	PDumpInit();
}

IMG_VOID PDumpDeInitCommon(IMG_VOID)
{
	
	FreeTempBuffer();

	
	PDumpDeInit();
}

#if defined(SGX_SUPPORT_COMMON_PDUMP)

IMG_BOOL PDumpIsSuspended(IMG_VOID)
{
	return PDumpOSIsSuspended();
}

PVRSRV_ERROR PDumpRegWithFlagsKM(IMG_UINT32 ui32Reg, IMG_UINT32 ui32Data, IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING()
	PDUMP_DBG(("PDumpRegWithFlagsKM"));
	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "WRW :SGXREG:0x%8.8lX 0x%8.8lX\r\n", ui32Reg, ui32Data);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpRegKM(IMG_UINT32 ui32Reg,IMG_UINT32 ui32Data)
{
	return PDumpRegWithFlagsKM(ui32Reg, ui32Data, PDUMP_FLAGS_CONTINUOUS);
}

PVRSRV_ERROR PDumpRegPolWithFlagsKM(IMG_UINT32 ui32RegAddr, IMG_UINT32 ui32RegValue, IMG_UINT32 ui32Mask, IMG_UINT32 ui32Flags)
{
	
	#define POLL_DELAY			1000UL
	#define POLL_COUNT_LONG		(2000000000UL / POLL_DELAY)
	#define POLL_COUNT_SHORT	(1000000UL / POLL_DELAY)

	PVRSRV_ERROR eErr;
	IMG_UINT32	ui32PollCount;

	PDUMP_GET_SCRIPT_STRING();
	PDUMP_DBG(("PDumpRegPolWithFlagsKM"));

	if (((ui32RegAddr == EUR_CR_EVENT_STATUS) &&
		(ui32RegValue & ui32Mask & EUR_CR_EVENT_STATUS_TA_FINISHED_MASK) != 0) ||
	    ((ui32RegAddr == EUR_CR_EVENT_STATUS) &&
		(ui32RegValue & ui32Mask & EUR_CR_EVENT_STATUS_PIXELBE_END_RENDER_MASK) != 0) ||
	    ((ui32RegAddr == EUR_CR_EVENT_STATUS) &&
		(ui32RegValue & ui32Mask & EUR_CR_EVENT_STATUS_DPM_3D_MEM_FREE_MASK) != 0))
	{
		ui32PollCount = POLL_COUNT_LONG;
	}
	else
	{
		ui32PollCount = POLL_COUNT_SHORT;
	}

	
	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "POL :SGXREG:0x%8.8lX 0x%8.8lX 0x%8.8lX %d %lu %d\r\n",
			ui32RegAddr, ui32RegValue, ui32Mask, 0, ui32PollCount, POLL_DELAY);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);

	return PVRSRV_OK;
}


PVRSRV_ERROR PDumpRegPolKM(IMG_UINT32 ui32RegAddr, IMG_UINT32 ui32RegValue, IMG_UINT32 ui32Mask)
{
	return PDumpRegPolWithFlagsKM(ui32RegAddr, ui32RegValue, ui32Mask, PDUMP_FLAGS_CONTINUOUS);
}

PVRSRV_ERROR PDumpMallocPages (PVRSRV_DEVICE_TYPE eDeviceType,
                           IMG_UINT32         ui32DevVAddr,
                           IMG_CPU_VIRTADDR   pvLinAddr,
                           IMG_HANDLE         hOSMemHandle,
                           IMG_UINT32         ui32NumBytes,
                           IMG_UINT32         ui32PageSize,
                           IMG_HANDLE         hUniqueTag)
{
	PVRSRV_ERROR eErr;
	IMG_PUINT8		pui8LinAddr;
    IMG_UINT32      ui32Offset;
	IMG_UINT32		ui32NumPages;
	IMG_DEV_PHYADDR	sDevPAddr;
	IMG_UINT32		ui32Page;

	PDUMP_GET_SCRIPT_STRING();

#if defined(LINUX)
	PVR_ASSERT(hOSMemHandle);
#else
	
	PVR_UNREFERENCED_PARAMETER(hOSMemHandle);
	PVR_ASSERT(((IMG_UINT32) pvLinAddr & (SGX_MMU_PAGE_MASK)) == 0);
#endif

	PVR_ASSERT(((IMG_UINT32) ui32DevVAddr & (SGX_MMU_PAGE_MASK)) == 0);
	PVR_ASSERT(((IMG_UINT32) ui32NumBytes & (SGX_MMU_PAGE_MASK)) == 0);

	

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "-- MALLOC :SGXMEM:VA_%8.8lX 0x%8.8lX %lu\r\n",
			ui32DevVAddr, ui32NumBytes, ui32PageSize);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

	

	pui8LinAddr = (IMG_PUINT8) pvLinAddr;
	ui32Offset = 0;
	ui32NumPages = ui32NumBytes / ui32PageSize;
	while (ui32NumPages)
	{
		ui32NumPages--;

		
		PDumpOSCPUVAddrToDevPAddr(eDeviceType,
				hOSMemHandle,
				ui32Offset,
				pui8LinAddr,
				ui32PageSize,
				&sDevPAddr);
		ui32Page = sDevPAddr.uiAddr / ui32PageSize;
		
		pui8LinAddr	+= ui32PageSize;
		ui32Offset += ui32PageSize;

		eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "MALLOC :SGXMEM:PA_%8.8lX%8.8lX %lu %lu 0x%8.8lX\r\n",
												(IMG_UINT32) hUniqueTag,
												ui32Page * ui32PageSize,
												ui32PageSize,
												ui32PageSize,
												ui32Page * ui32PageSize);
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpMallocPageTable (PVRSRV_DEVICE_TYPE eDeviceType,
                               IMG_CPU_VIRTADDR   pvLinAddr,
								IMG_UINT32        ui32PTSize,
                               IMG_HANDLE         hUniqueTag)
{
	PVRSRV_ERROR eErr;
	IMG_DEV_PHYADDR	sDevPAddr;
	IMG_UINT32		ui32Page;

	PDUMP_GET_SCRIPT_STRING();

	PVR_ASSERT(((IMG_UINT32) pvLinAddr & (ui32PTSize - 1)) == 0);

	

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "-- MALLOC :SGXMEM:PAGE_TABLE 0x%8.8lX %lu\r\n", ui32PTSize, SGX_MMU_PAGE_SIZE);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

	

	

	
	
	
	

	
	

	
	{
		
		
		PDumpOSCPUVAddrToDevPAddr(eDeviceType,
				IMG_NULL,
				0,
				(IMG_PUINT8) pvLinAddr,
				SGX_MMU_PAGE_SIZE,
				&sDevPAddr);
		ui32Page = sDevPAddr.uiAddr >> SGX_MMU_PAGE_SHIFT;

		eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "MALLOC :SGXMEM:PA_%8.8lX%8.8lX 0x%lX %lu 0x%8.8lX\r\n",
												(IMG_UINT32) hUniqueTag,
												ui32Page * SGX_MMU_PAGE_SIZE,
												SGX_MMU_PAGE_SIZE,
												SGX_MMU_PAGE_SIZE,
												ui32Page * SGX_MMU_PAGE_SIZE);
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
		
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpFreePages	(BM_HEAP 			*psBMHeap,
                         IMG_DEV_VIRTADDR  sDevVAddr,
                         IMG_UINT32        ui32NumBytes,
                         IMG_UINT32        ui32PageSize,
                         IMG_HANDLE        hUniqueTag,
						 IMG_BOOL		   bInterleaved)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32 ui32NumPages, ui32PageCounter;
	IMG_DEV_PHYADDR	sDevPAddr;
	PVRSRV_DEVICE_NODE *psDeviceNode;

	PDUMP_GET_SCRIPT_STRING();

	PVR_ASSERT(((IMG_UINT32) sDevVAddr.uiAddr & (ui32PageSize - 1)) == 0);
	PVR_ASSERT(((IMG_UINT32) ui32NumBytes & (ui32PageSize - 1)) == 0);

	

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "-- FREE :SGXMEM:VA_%8.8lX\r\n", sDevVAddr.uiAddr);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

	

	ui32NumPages = ui32NumBytes / ui32PageSize;
	psDeviceNode = psBMHeap->pBMContext->psDeviceNode;
	for (ui32PageCounter = 0; ui32PageCounter < ui32NumPages; ui32PageCounter++)
	{
		if (!bInterleaved || (ui32PageCounter % 2) == 0)
		{
			sDevPAddr = psDeviceNode->pfnMMUGetPhysPageAddr(psBMHeap->pMMUHeap, sDevVAddr);

			eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "FREE :SGXMEM:PA_%8.8lX%8.8lX\r\n", (IMG_UINT32) hUniqueTag, sDevPAddr.uiAddr);
			if(eErr != PVRSRV_OK)
			{
				return eErr;
			}
			PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
		}
		else
		{
			
		}

		sDevVAddr.uiAddr += ui32PageSize;
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpFreePageTable	(PVRSRV_DEVICE_TYPE eDeviceType,
							 IMG_CPU_VIRTADDR   pvLinAddr,
							 IMG_UINT32         ui32PTSize,
							 IMG_HANDLE         hUniqueTag)
{
	PVRSRV_ERROR eErr;
	IMG_DEV_PHYADDR	sDevPAddr;
	IMG_UINT32		ui32Page;

	PDUMP_GET_SCRIPT_STRING();

	PVR_UNREFERENCED_PARAMETER(ui32PTSize);

	
	PVR_ASSERT(((IMG_UINT32) pvLinAddr & (ui32PTSize-1UL)) == 0);	

	

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "-- FREE :SGXMEM:PAGE_TABLE\r\n");
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

	

	
	
	
	

	
	

	
	{
		PDumpOSCPUVAddrToDevPAddr(eDeviceType,
				IMG_NULL,
				0,
				(IMG_PUINT8) pvLinAddr,
				SGX_MMU_PAGE_SIZE,
				&sDevPAddr);
		ui32Page = sDevPAddr.uiAddr >> SGX_MMU_PAGE_SHIFT;

		eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "FREE :SGXMEM:PA_%8.8lX%8.8lX\r\n", (IMG_UINT32) hUniqueTag, ui32Page * SGX_MMU_PAGE_SIZE);
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpPDRegWithFlags(IMG_UINT32 ui32Reg,
							 IMG_UINT32 ui32Data,
							 IMG_UINT32	ui32Flags,
							 IMG_HANDLE hUniqueTag)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING()

	

#if defined(SGX_FEATURE_36BIT_MMU)
	eErr = PDumpOSBufprintf(hScript, ui32MaxLen,
			 "WRW :SGXMEM:$1 :SGXMEM:PA_%8.8lX%8.8lX:0x0\r\n",
			 (IMG_UINT32)hUniqueTag,
			 (ui32Data & SGX_MMU_PDE_ADDR_MASK) << SGX_MMU_PDE_ADDR_ALIGNSHIFT);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);
	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "SHR :SGXMEM:$1 :SGXMEM:$1 0x4\r\n");
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);
	eErr = PDumpOSBufprintf(hScript, ui32MaxLen,
			 "WRW :SGXREG:0x%8.8lX: SGXMEM:$1\r\n",
			 ui32Reg);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);
#else
	eErr = PDumpOSBufprintf(hScript,
				ui32MaxLen,
				"WRW :SGXREG:0x%8.8lX :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX\r\n",
				ui32Reg,
				(IMG_UINT32) hUniqueTag,
				(ui32Data & SGX_MMU_PDE_ADDR_MASK) << SGX_MMU_PDE_ADDR_ALIGNSHIFT,
				ui32Data & ~SGX_MMU_PDE_ADDR_MASK);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);
#endif
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpPDReg	(IMG_UINT32 ui32Reg,
					 IMG_UINT32 ui32Data,
					 IMG_HANDLE hUniqueTag)
{
	return PDumpPDRegWithFlags(ui32Reg, ui32Data, PDUMP_FLAGS_CONTINUOUS, hUniqueTag);
}

PVRSRV_ERROR PDumpMemPolKM(PVRSRV_KERNEL_MEM_INFO		*psMemInfo,
						   IMG_UINT32			ui32Offset,
						   IMG_UINT32			ui32Value,
						   IMG_UINT32			ui32Mask,
						   PDUMP_POLL_OPERATOR	eOperator,
						   IMG_UINT32			ui32Flags,
						   IMG_HANDLE			hUniqueTag)
{
	#define MEMPOLL_DELAY		(1000)
	#define MEMPOLL_COUNT		(2000000000 / MEMPOLL_DELAY)

	PVRSRV_ERROR eErr;
	IMG_UINT32			ui32PageOffset;
	IMG_UINT8			*pui8LinAddr;
	IMG_DEV_PHYADDR		sDevPAddr;
	IMG_DEV_VIRTADDR	sDevVPageAddr;
	PDUMP_GET_SCRIPT_STRING();

	
	PVR_ASSERT((ui32Offset + sizeof(IMG_UINT32)) <= psMemInfo->ui32AllocSize);

	

	eErr = PDumpOSBufprintf(hScript,
			 ui32MaxLen,
			 "-- POL :SGXMEM:VA_%8.8lX 0x%8.8lX 0x%8.8lX %d %d %d\r\n",
			 psMemInfo->sDevVAddr.uiAddr + ui32Offset,
			 ui32Value,
			 ui32Mask,
			 eOperator,
			 MEMPOLL_COUNT,
			 MEMPOLL_DELAY);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);


	pui8LinAddr = psMemInfo->pvLinAddrKM;

	
	pui8LinAddr += ui32Offset;

	


	PDumpOSCPUVAddrToPhysPages(psMemInfo->sMemBlk.hOSMemHandle,
			ui32Offset,
			pui8LinAddr,
			&ui32PageOffset);

	
	sDevVPageAddr.uiAddr = psMemInfo->sDevVAddr.uiAddr + ui32Offset - ui32PageOffset;

	PVR_ASSERT((sDevVPageAddr.uiAddr & 0xFFF) == 0);

	
	BM_GetPhysPageAddr(psMemInfo, sDevVPageAddr, &sDevPAddr);

	
	sDevPAddr.uiAddr += ui32PageOffset;

	eErr = PDumpOSBufprintf(hScript,
			 ui32MaxLen,
			 "POL :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX 0x%8.8lX 0x%8.8lX %d %d %d\r\n",
			 (IMG_UINT32) hUniqueTag,
			 sDevPAddr.uiAddr & ~(SGX_MMU_PAGE_MASK),
			 sDevPAddr.uiAddr & (SGX_MMU_PAGE_MASK),
			 ui32Value,
			 ui32Mask,
			 eOperator,
			 MEMPOLL_COUNT,
			 MEMPOLL_DELAY);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpMemKM(IMG_PVOID pvAltLinAddr,
						PVRSRV_KERNEL_MEM_INFO *psMemInfo,
						IMG_UINT32 ui32Offset,
						IMG_UINT32 ui32Bytes,
						IMG_UINT32 ui32Flags,
						IMG_HANDLE hUniqueTag)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32 ui32NumPages;
	IMG_UINT32 ui32PageByteOffset;
	IMG_UINT32 ui32BlockBytes;
	IMG_UINT8* pui8LinAddr;
	IMG_UINT8* pui8DataLinAddr = IMG_NULL;
	IMG_DEV_VIRTADDR sDevVPageAddr;
	IMG_DEV_VIRTADDR sDevVAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_UINT32 ui32ParamOutPos;

	PDUMP_GET_SCRIPT_AND_FILE_STRING();
	

	PVR_ASSERT((ui32Offset + ui32Bytes) <= psMemInfo->ui32AllocSize);

	if (!PDumpOSJTInitialised())
	{
		return PVRSRV_ERROR_GENERIC;
	}

	if (ui32Bytes == 0 || PDumpOSIsSuspended())
	{
		return PVRSRV_OK;
	}

	
	if(pvAltLinAddr)
	{
		pui8DataLinAddr = pvAltLinAddr;
	}
	else if(psMemInfo->pvLinAddrKM)
	{
		pui8DataLinAddr = (IMG_UINT8 *)psMemInfo->pvLinAddrKM + ui32Offset;
	}
	pui8LinAddr = (IMG_UINT8 *)psMemInfo->pvLinAddrKM;
	sDevVAddr = psMemInfo->sDevVAddr;

	
	sDevVAddr.uiAddr += ui32Offset;
	pui8LinAddr += ui32Offset;

	PVR_ASSERT(pui8DataLinAddr);

	PDumpOSCheckForSplitting(PDumpOSGetStream(PDUMP_STREAM_PARAM2), ui32Bytes, ui32Flags);

	ui32ParamOutPos = PDumpOSGetStreamOffset(PDUMP_STREAM_PARAM2);

	

	if(!PDumpOSWriteString(PDumpOSGetStream(PDUMP_STREAM_PARAM2),
						pui8DataLinAddr,
						ui32Bytes,
						ui32Flags))
	{
		return PVRSRV_ERROR_GENERIC;
	}

	if (PDumpOSGetParamFileNum() == 0)
	{
		eErr = PDumpOSSprintf(pszFileName, ui32MaxLenFileName, "%%0%%.prm");
	}
	else
	{
		eErr = PDumpOSSprintf(pszFileName, ui32MaxLenFileName, "%%0%%%lu.prm", PDumpOSGetParamFileNum());
	}
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	

	eErr = PDumpOSBufprintf(hScript,
			 ui32MaxLenScript,
			 "-- LDB :SGXMEM:VA_%8.8lX%8.8lX:0x%8.8lX 0x%8.8lX 0x%8.8lX %s\r\n",
			 (IMG_UINT32)hUniqueTag,
			 psMemInfo->sDevVAddr.uiAddr,
			 ui32Offset,
			 ui32Bytes,
			 ui32ParamOutPos,
			 pszFileName);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);

	


	PDumpOSCPUVAddrToPhysPages(psMemInfo->sMemBlk.hOSMemHandle,
			ui32Offset,
			pui8LinAddr,
			&ui32PageByteOffset);
	ui32NumPages = (ui32PageByteOffset + ui32Bytes + HOST_PAGESIZE() - 1) / HOST_PAGESIZE();

	while(ui32NumPages)
	{
#if 0
		IMG_UINT32 ui32BlockBytes = MIN(ui32BytesRemaining, PAGE_SIZE);
		CpuPAddr = OSMemHandleToCpuPAddr(psMemInfo->sMemBlk.hOSMemHandle,
						 ui32CurrentOffset);
#endif
		ui32NumPages--;
	
		
		sDevVPageAddr.uiAddr = sDevVAddr.uiAddr - ui32PageByteOffset;

		PVR_ASSERT((sDevVPageAddr.uiAddr & 0xFFF) == 0);

		
		BM_GetPhysPageAddr(psMemInfo, sDevVPageAddr, &sDevPAddr);

		
		sDevPAddr.uiAddr += ui32PageByteOffset;
#if 0
		if(ui32PageByteOffset)
		{
		    ui32BlockBytes =
			MIN(ui32BytesRemaining, PAGE_ALIGN(CpuPAddr.uiAddr) - CpuPAddr.uiAddr);
		    
		    ui32PageByteOffset = 0;
		}
#endif
		
		if (ui32PageByteOffset + ui32Bytes > HOST_PAGESIZE())
		{
			
			ui32BlockBytes = HOST_PAGESIZE() - ui32PageByteOffset;
		}
		else
		{
			
			ui32BlockBytes = ui32Bytes;
		}

		eErr = PDumpOSBufprintf(hScript,
					 ui32MaxLenScript,
					 "LDB :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX 0x%8.8lX 0x%8.8lX %s\r\n",
					 (IMG_UINT32) hUniqueTag,
					 sDevPAddr.uiAddr & ~(SGX_MMU_PAGE_MASK),
					 sDevPAddr.uiAddr & (SGX_MMU_PAGE_MASK),
					 ui32BlockBytes,
					 ui32ParamOutPos,
					 pszFileName);
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, ui32Flags);

		

		
		ui32PageByteOffset = 0;
		
		ui32Bytes -= ui32BlockBytes;	 
		
		sDevVAddr.uiAddr += ui32BlockBytes;
		
		pui8LinAddr += ui32BlockBytes;
		
		ui32ParamOutPos += ui32BlockBytes;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpMem2KM(PVRSRV_DEVICE_TYPE eDeviceType,
						 IMG_CPU_VIRTADDR pvLinAddr,
						 IMG_UINT32 ui32Bytes,
						 IMG_UINT32 ui32Flags,
						 IMG_BOOL bInitialisePages,
						 IMG_HANDLE hUniqueTag1,
						 IMG_HANDLE hUniqueTag2)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32 ui32NumPages;
	IMG_UINT32 ui32PageOffset;
	IMG_UINT32 ui32BlockBytes;
	IMG_UINT8* pui8LinAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_UINT32 ui32Offset;
	IMG_UINT32 ui32ParamOutPos;

	PDUMP_GET_SCRIPT_AND_FILE_STRING();

	if (!pvLinAddr || !PDumpOSJTInitialised())
	{
		return PVRSRV_ERROR_GENERIC;
	}

	if (PDumpOSIsSuspended())
	{
		return PVRSRV_OK;
	}

	PDumpOSCheckForSplitting(PDumpOSGetStream(PDUMP_STREAM_PARAM2), ui32Bytes, ui32Flags);

	ui32ParamOutPos = PDumpOSGetStreamOffset(PDUMP_STREAM_PARAM2);

	if (bInitialisePages)
	{
		


		if (!PDumpOSWriteString(PDumpOSGetStream(PDUMP_STREAM_PARAM2),
							pvLinAddr,
							ui32Bytes,
							PDUMP_FLAGS_CONTINUOUS))
		{
			return PVRSRV_ERROR_GENERIC;
		}

		if (PDumpOSGetParamFileNum() == 0)
		{
			eErr = PDumpOSSprintf(pszFileName, ui32MaxLenFileName, "%%0%%.prm");
		}
		else
		{
			eErr = PDumpOSSprintf(pszFileName, ui32MaxLenFileName, "%%0%%%lu.prm", PDumpOSGetParamFileNum());
		}
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
	}

	

	
	ui32PageOffset	= (IMG_UINT32) pvLinAddr & (HOST_PAGESIZE() - 1);
	ui32NumPages	= (ui32PageOffset + ui32Bytes + HOST_PAGESIZE() - 1) / HOST_PAGESIZE();
	pui8LinAddr		= (IMG_UINT8*) pvLinAddr;

	while (ui32NumPages)
	{
		ui32NumPages--;
		sCpuPAddr = OSMapLinToCPUPhys(pui8LinAddr);
		sDevPAddr = SysCpuPAddrToDevPAddr(eDeviceType, sCpuPAddr);

		
		if (ui32PageOffset + ui32Bytes > HOST_PAGESIZE())
		{
			
			ui32BlockBytes = HOST_PAGESIZE() - ui32PageOffset;
		}
		else
		{
			
			ui32BlockBytes = ui32Bytes;
		}

		

		if (bInitialisePages)
		{
			eErr = PDumpOSBufprintf(hScript,
					 ui32MaxLenScript,
					 "LDB :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX 0x%8.8lX 0x%8.8lX %s\r\n",
					 (IMG_UINT32) hUniqueTag1,
					 sDevPAddr.uiAddr & ~(SGX_MMU_PAGE_MASK),
					 sDevPAddr.uiAddr & (SGX_MMU_PAGE_MASK),
					 ui32BlockBytes,
					 ui32ParamOutPos,
					 pszFileName);
			if(eErr != PVRSRV_OK)
			{
				return eErr;
			}
			PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
		}
		else
		{
			for (ui32Offset = 0; ui32Offset < ui32BlockBytes; ui32Offset += sizeof(IMG_UINT32))
			{
				IMG_UINT32 ui32PTE = *((IMG_UINT32 *) (pui8LinAddr + ui32Offset));  

				if ((ui32PTE & SGX_MMU_PDE_ADDR_MASK) != 0)
				{
#if defined(SGX_FEATURE_36BIT_MMU)
					eErr = PDumpOSBufprintf(hScript,
							ui32MaxLenScript,
							 "WRW :SGXMEM:$1 :SGXMEM:PA_%8.8lX%8.8lX:0x0\r\n",
							 (IMG_UINT32)hUniqueTag2,
							 (ui32PTE & SGX_MMU_PDE_ADDR_MASK) << SGX_MMU_PTE_ADDR_ALIGNSHIFT);
					if(eErr != PVRSRV_OK)
					{
						return eErr;
					}
					PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
					eErr = PDumpOSBufprintf(hScript, ui32MaxLenScript, "SHR :SGXMEM:$1 :SGXMEM:$1 0x4\r\n");
					if(eErr != PVRSRV_OK)
					{
						return eErr;
					}
					PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
					eErr = PDumpOSBufprintf(hScript, ui32MaxLenScript, "OR :SGXMEM:$1 :SGXMEM:$1 0x%8.8lX\r\n", ui32PTE & ~SGX_MMU_PDE_ADDR_MASK);
					if(eErr != PVRSRV_OK)
					{
						return eErr;
					}
					PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
					eErr = PDumpOSBufprintf(hScript,
							ui32MaxLenScript,
							 "WRW :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX :SGXMEM:$1\r\n",
							 (IMG_UINT32)hUniqueTag1,
							 (sDevPAddr.uiAddr + ui32Offset) & ~(SGX_MMU_PAGE_MASK),
							 (sDevPAddr.uiAddr + ui32Offset) & (SGX_MMU_PAGE_MASK));
					if(eErr != PVRSRV_OK)
					{
						return eErr;
					}
					PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
#else
					eErr = PDumpOSBufprintf(hScript,
							ui32MaxLenScript,
							 "WRW :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX\r\n",
							 (IMG_UINT32) hUniqueTag1,
							 (sDevPAddr.uiAddr + ui32Offset) & ~(SGX_MMU_PAGE_MASK),
							 (sDevPAddr.uiAddr + ui32Offset) & (SGX_MMU_PAGE_MASK),
							 (IMG_UINT32) hUniqueTag2,
							 (ui32PTE & SGX_MMU_PDE_ADDR_MASK) << SGX_MMU_PTE_ADDR_ALIGNSHIFT,
							 ui32PTE & ~SGX_MMU_PDE_ADDR_MASK);
					if(eErr != PVRSRV_OK)
					{
						return eErr;
					}
#endif
				}
				else
				{
					PVR_ASSERT((ui32PTE & SGX_MMU_PTE_VALID) == 0UL);
					eErr = PDumpOSBufprintf(hScript,
							ui32MaxLenScript,
							 "WRW :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX 0x%8.8lX%8.8lX\r\n",
							 (IMG_UINT32) hUniqueTag1,
							 (sDevPAddr.uiAddr + ui32Offset) & ~(SGX_MMU_PAGE_MASK),
							 (sDevPAddr.uiAddr + ui32Offset) & (SGX_MMU_PAGE_MASK),
							 (ui32PTE << SGX_MMU_PTE_ADDR_ALIGNSHIFT),
							 (IMG_UINT32) hUniqueTag2);
					if(eErr != PVRSRV_OK)
					{
						return eErr;
					}
				}
				PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
			}
		}

		

		
		ui32PageOffset = 0;
		
		ui32Bytes -= ui32BlockBytes;
		
		pui8LinAddr += ui32BlockBytes;
		
		ui32ParamOutPos += ui32BlockBytes;
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpPDDevPAddrKM(PVRSRV_KERNEL_MEM_INFO *psMemInfo,
							   IMG_UINT32 ui32Offset,
							   IMG_DEV_PHYADDR sPDDevPAddr,
							   IMG_HANDLE hUniqueTag1,
							   IMG_HANDLE hUniqueTag2)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32 ui32PageByteOffset;
	IMG_DEV_VIRTADDR sDevVAddr;
	IMG_DEV_VIRTADDR sDevVPageAddr;
	IMG_DEV_PHYADDR sDevPAddr;

	PDUMP_GET_SCRIPT_STRING();

	if(!PDumpOSWriteString(PDumpOSGetStream(PDUMP_STREAM_PARAM2),
						(IMG_UINT8 *)&sPDDevPAddr,
						sizeof(IMG_DEV_PHYADDR),
						PDUMP_FLAGS_CONTINUOUS))
	{
		return PVRSRV_ERROR_GENERIC;
	}

	sDevVAddr = psMemInfo->sDevVAddr;
	ui32PageByteOffset = sDevVAddr.uiAddr & (SGX_MMU_PAGE_MASK);

	sDevVPageAddr.uiAddr = sDevVAddr.uiAddr - ui32PageByteOffset;
	PVR_ASSERT((sDevVPageAddr.uiAddr & 0xFFF) == 0);

	BM_GetPhysPageAddr(psMemInfo, sDevVPageAddr, &sDevPAddr);
	sDevPAddr.uiAddr += ui32PageByteOffset + ui32Offset;

	if ((sPDDevPAddr.uiAddr & SGX_MMU_PDE_ADDR_MASK) != 0UL)
	{
#if defined(SGX_FEATURE_36BIT_MMU)
		eErr = PDumpOSBufprintf(hScript,
				ui32MaxLen,
				 "WRW :SGXMEM:$1 :SGXMEM:PA_%8.8lX%8.8lX:0x0\r\n",
				 (IMG_UINT32)hUniqueTag2,
				 sPDDevPAddr.uiAddr);
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

		eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "AND  :SGXMEM:$2 :SGXMEM:$1 0xFFFFFFFF\r\n");
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

		eErr = PDumpOSBufprintf(hScript,
				ui32MaxLen,
				 "WRW :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX :SGXMEM:$2\r\n",
				 (IMG_UINT32)hUniqueTag1,
				 (sDevPAddr.uiAddr) & ~(SGX_MMU_PAGE_MASK),
				 (sDevPAddr.uiAddr) & (SGX_MMU_PAGE_MASK));
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

		eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "SHR :SGXMEM:$2 :SGXMEM:$1 0x20\r\n");
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

		eErr = PDumpOSBufprintf(hScript,
				ui32MaxLen,
				 "WRW :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX :SGXMEM:$2\r\n",
				 (IMG_UINT32)hUniqueTag1,
				 (sDevPAddr.uiAddr + 4) & ~(SGX_MMU_PAGE_MASK),
				 (sDevPAddr.uiAddr + 4) & (SGX_MMU_PAGE_MASK));
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
		PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);
#else
		eErr = PDumpOSBufprintf(hScript,
				 ui32MaxLen,
				 "WRW :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX\r\n",
				 (IMG_UINT32) hUniqueTag1,
				 sDevPAddr.uiAddr & ~(SGX_MMU_PAGE_MASK),
				 sDevPAddr.uiAddr & (SGX_MMU_PAGE_MASK),
				 (IMG_UINT32) hUniqueTag2,
				 sPDDevPAddr.uiAddr & SGX_MMU_PDE_ADDR_MASK,
				 sPDDevPAddr.uiAddr & ~SGX_MMU_PDE_ADDR_MASK);
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
#endif
	}
	else
	{
		PVR_ASSERT(!(sDevPAddr.uiAddr & SGX_MMU_PTE_VALID));
		eErr = PDumpOSBufprintf(hScript,
				 ui32MaxLen,
				 "WRW :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX 0x%8.8lX\r\n",
				 (IMG_UINT32) hUniqueTag1,
				 sDevPAddr.uiAddr & ~(SGX_MMU_PAGE_MASK),
				 sDevPAddr.uiAddr & (SGX_MMU_PAGE_MASK),
				 sPDDevPAddr.uiAddr);
		if(eErr != PVRSRV_OK)
		{
			return eErr;
		}
	}
	PDumpOSWriteString2(hScript, PDUMP_FLAGS_CONTINUOUS);

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpCommentKM(IMG_CHAR *pszComment, IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_MSG_STRING();
	PDUMP_DBG(("PDumpCommentKM"));

	
	if (!PDumpOSWriteString2("-- ", ui32Flags))
	{
		if(ui32Flags & PDUMP_FLAGS_CONTINUOUS)
		{
			return PVRSRV_ERROR_GENERIC;
		}
		else
		{
			return PVRSRV_ERROR_CMD_NOT_PROCESSED;
		}
	}

	
	eErr = PDumpOSBufprintf(hMsg, ui32MaxLen, "%s", pszComment);
	if( (eErr != PVRSRV_OK) &&
		(eErr != PVRSRV_ERROR_PDUMP_BUF_OVERFLOW))
	{
		return eErr;
	}

	
	PDumpOSVerifyLineEnding(hMsg, ui32MaxLen);
	PDumpOSWriteString2(hMsg, ui32Flags);

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpCommentWithFlags(IMG_UINT32 ui32Flags, IMG_CHAR * pszFormat, ...)
{
	PVRSRV_ERROR eErr;
	PDUMP_va_list ap;
	PDUMP_GET_MSG_STRING();

	
	PDUMP_va_start(ap, pszFormat);
	eErr = PDumpOSVSprintf(hMsg, ui32MaxLen, pszFormat, ap);
	PDUMP_va_end(ap);

	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	return PDumpCommentKM(hMsg, ui32Flags);
}

PVRSRV_ERROR PDumpComment(IMG_CHAR *pszFormat, ...)
{
	PVRSRV_ERROR eErr;
	PDUMP_va_list ap;
	PDUMP_GET_MSG_STRING();

	
	PDUMP_va_start(ap, pszFormat);
	eErr = PDumpOSVSprintf(hMsg, ui32MaxLen, pszFormat, ap);
	PDUMP_va_end(ap);

	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	return PDumpCommentKM(hMsg, PDUMP_FLAGS_CONTINUOUS);
}

PVRSRV_ERROR PDumpDriverInfoKM(IMG_CHAR *pszString, IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32	ui32MsgLen;
	PDUMP_GET_MSG_STRING();

	
	eErr = PDumpOSBufprintf(hMsg, ui32MaxLen, "%s", pszString);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	
	PDumpOSVerifyLineEnding(hMsg, ui32MaxLen);
	ui32MsgLen = PDumpOSBuflen(hMsg, ui32MaxLen);

	if	(!PDumpOSWriteString(PDumpOSGetStream(PDUMP_STREAM_DRIVERINFO),
						  (IMG_UINT8 *)hMsg,
						  ui32MsgLen,
						  ui32Flags))
	{
		if	(ui32Flags & PDUMP_FLAGS_CONTINUOUS)
		{
			return PVRSRV_ERROR_GENERIC;
		}
		else
		{
			return PVRSRV_ERROR_CMD_NOT_PROCESSED;
		}
	}

	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpBitmapKM(	IMG_CHAR *pszFileName,
							IMG_UINT32 ui32FileOffset,
							IMG_UINT32 ui32Width,
							IMG_UINT32 ui32Height,
							IMG_UINT32 ui32StrideInBytes,
							IMG_DEV_VIRTADDR sDevBaseAddr,
							IMG_UINT32 ui32Size,
							PDUMP_PIXEL_FORMAT ePixelFormat,
							PDUMP_MEM_FORMAT eMemFormat,
							IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();
	PDumpCommentWithFlags(ui32PDumpFlags, "\r\n-- Dump bitmap of render\r\n");

#if defined(SGX_FEATURE_MULTIPLE_MEM_CONTEXTS)
	
	eErr = PDumpOSBufprintf(hScript,
				ui32MaxLen,
				"SII %s %s.bin :SGXMEM:v%x:0x%08lX 0x%08lX 0x%08lX 0x%08X 0x%08lX 0x%08lX 0x%08lX 0x%08X\r\n",
				pszFileName,
				pszFileName,
				PDUMP_DATAMASTER_PIXEL,
				sDevBaseAddr.uiAddr,
				ui32Size,
				ui32FileOffset,
				ePixelFormat,
				ui32Width,
				ui32Height,
				ui32StrideInBytes,
				eMemFormat);
#else
	eErr = PDumpOSBufprintf(hScript,
				ui32MaxLen,
				"SII %s %s.bin :SGXMEM:v:0x%08lX 0x%08lX 0x%08lX 0x%08X 0x%08lX 0x%08lX 0x%08lX 0x%08X\r\n",
				pszFileName,
				pszFileName,
				sDevBaseAddr.uiAddr,
				ui32Size,
				ui32FileOffset,
				ePixelFormat,
				ui32Width,
				ui32Height,
				ui32StrideInBytes,
				eMemFormat);
#endif
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDumpOSWriteString2( hScript, ui32PDumpFlags);
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpReadRegKM		(	IMG_CHAR *pszFileName,
									IMG_UINT32 ui32FileOffset,
									IMG_UINT32 ui32Address,
									IMG_UINT32 ui32Size,
									IMG_UINT32 ui32PDumpFlags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();

	PVR_UNREFERENCED_PARAMETER(ui32Size);

	eErr = PDumpOSBufprintf(hScript,
			ui32MaxLen,
			"SAB :SGXREG:0x%08lX 0x%08lX %s\r\n",
			ui32Address,
			ui32FileOffset,
			pszFileName);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDumpOSWriteString2( hScript, ui32PDumpFlags);

	return PVRSRV_OK;
}

IMG_BOOL PDumpTestNextFrame(IMG_UINT32 ui32CurrentFrame)
{
	IMG_BOOL	bFrameDumped;

	

	(IMG_VOID) PDumpSetFrameKM(ui32CurrentFrame + 1);
	bFrameDumped = PDumpIsCaptureFrameKM();
	(IMG_VOID) PDumpSetFrameKM(ui32CurrentFrame);

	return bFrameDumped;
}

static PVRSRV_ERROR PDumpSignatureRegister	(IMG_CHAR	*pszFileName,
									 IMG_UINT32		ui32Address,
									 IMG_UINT32		ui32Size,
									 IMG_UINT32		*pui32FileOffset,
									 IMG_UINT32		ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();

	eErr = PDumpOSBufprintf(hScript,
			ui32MaxLen,
			"SAB :SGXREG:0x%08X 0x%08X %s\r\n",
			ui32Address,
			*pui32FileOffset,
			pszFileName);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDumpOSWriteString2(hScript, ui32Flags);
	*pui32FileOffset += ui32Size;
	return PVRSRV_OK;
}

static IMG_VOID PDumpRegisterRange(IMG_CHAR *pszFileName,
		IMG_UINT32 *pui32Registers,
		IMG_UINT32  ui32NumRegisters,
		IMG_UINT32 *pui32FileOffset,
		IMG_UINT32	ui32Size,
		IMG_UINT32	ui32Flags)
{
	IMG_UINT32 i;
	for (i = 0; i < ui32NumRegisters; i++)
	{
		PDumpSignatureRegister(pszFileName, pui32Registers[i], ui32Size, pui32FileOffset, ui32Flags);
	}
}

PVRSRV_ERROR PDump3DSignatureRegisters(IMG_UINT32 ui32DumpFrameNum,
			IMG_BOOL bLastFrame,
			IMG_UINT32 *pui32Registers,
			IMG_UINT32 ui32NumRegisters)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32	ui32FileOffset, ui32Flags;

	PDUMP_GET_FILE_STRING();

	ui32Flags = bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0;
	ui32FileOffset = 0;

	PDumpCommentWithFlags(ui32Flags, "\r\n-- Dump 3D signature registers\r\n");
	eErr = PDumpOSSprintf(pszFileName, ui32MaxLen, "out%lu_3d.sig", ui32DumpFrameNum);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDumpRegisterRange(pszFileName, pui32Registers, ui32NumRegisters, &ui32FileOffset, sizeof(IMG_UINT32), ui32Flags);
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpTASignatureRegisters	(IMG_UINT32 ui32DumpFrameNum,
			 IMG_UINT32	ui32TAKickCount,
			 IMG_BOOL	bLastFrame,
			 IMG_UINT32 *pui32Registers,
			 IMG_UINT32 ui32NumRegisters)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32	ui32FileOffset, ui32Flags;

	PDUMP_GET_FILE_STRING();

	ui32Flags = bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0;
	ui32FileOffset = ui32TAKickCount * ui32NumRegisters * sizeof(IMG_UINT32);

	PDumpCommentWithFlags(ui32Flags, "\r\n-- Dump TA signature registers\r\n");
	eErr = PDumpOSSprintf(pszFileName, ui32MaxLen, "out%lu_ta.sig", ui32DumpFrameNum);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDumpRegisterRange(pszFileName, pui32Registers, ui32NumRegisters, &ui32FileOffset, sizeof(IMG_UINT32), ui32Flags);
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpCounterRegisters (IMG_UINT32 ui32DumpFrameNum,
								IMG_BOOL	bLastFrame,
								IMG_UINT32 *pui32Registers,
								IMG_UINT32 ui32NumRegisters)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32	ui32FileOffset, ui32Flags;

	PDUMP_GET_FILE_STRING();

	ui32Flags = bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0UL;
	ui32FileOffset = 0UL;

	PDumpCommentWithFlags(ui32Flags, "\r\n-- Dump counter registers\r\n");
	eErr = PDumpOSSprintf(pszFileName, ui32MaxLen, "out%lu.perf", ui32DumpFrameNum);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDumpRegisterRange(pszFileName, pui32Registers, ui32NumRegisters, &ui32FileOffset, sizeof(IMG_UINT32), ui32Flags);
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpRegRead(const IMG_UINT32 ui32RegOffset, IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "RDW :SGXREG:0x%lX\r\n", ui32RegOffset);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpCycleCountRegRead(const IMG_UINT32 ui32RegOffset, IMG_BOOL bLastFrame)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "RDW :SGXREG:0x%lX\r\n", ui32RegOffset);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, bLastFrame ? PDUMP_FLAGS_LASTFRAME : 0);
	return PVRSRV_OK;
}

PVRSRV_ERROR PDumpHWPerfCBKM (IMG_CHAR			*pszFileName,
						  IMG_UINT32		ui32FileOffset,
						  IMG_DEV_VIRTADDR	sDevBaseAddr,
						  IMG_UINT32 		ui32Size,
						  IMG_UINT32 		ui32PDumpFlags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();
	PDumpCommentWithFlags(ui32PDumpFlags, "\r\n-- Dump Hardware Performance Circular Buffer\r\n");

	eErr = PDumpOSBufprintf(hScript,
				ui32MaxLen,
#if defined(SGX_FEATURE_MULTIPLE_MEM_CONTEXTS)
				"SAB :SGXMEM:v%x:0x%08lX 0x%08lX 0x%08lX %s.bin\r\n",
				PDUMP_DATAMASTER_EDM,
#else
				"SAB :SGXMEM:v:0x%08lX 0x%08lX 0x%08lX %s.bin\r\n",
#endif
				sDevBaseAddr.uiAddr,
				ui32Size,
				ui32FileOffset,
				pszFileName);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}

	PDumpOSWriteString2(hScript, ui32PDumpFlags);
	return PVRSRV_OK;
}


PVRSRV_ERROR PDumpCBP(PPVRSRV_KERNEL_MEM_INFO		psROffMemInfo,
			  IMG_UINT32					ui32ROffOffset,
			  IMG_UINT32					ui32WPosVal,
			  IMG_UINT32					ui32PacketSize,
			  IMG_UINT32					ui32BufferSize,
			  IMG_UINT32					ui32Flags,
			  IMG_HANDLE					hUniqueTag)
{
	PVRSRV_ERROR eErr;
	IMG_UINT32			ui32PageOffset;
	IMG_UINT8			*pui8LinAddr;
	IMG_DEV_VIRTADDR	sDevVAddr;
	IMG_DEV_PHYADDR		sDevPAddr;
	IMG_DEV_VIRTADDR 	sDevVPageAddr;
    

	PDUMP_GET_SCRIPT_STRING();

	
	PVR_ASSERT((ui32ROffOffset + sizeof(IMG_UINT32)) <= psROffMemInfo->ui32AllocSize);

	pui8LinAddr = psROffMemInfo->pvLinAddrKM;
	sDevVAddr = psROffMemInfo->sDevVAddr;

	
	pui8LinAddr += ui32ROffOffset;
	sDevVAddr.uiAddr += ui32ROffOffset;

	


    
    
	PDumpOSCPUVAddrToPhysPages(psROffMemInfo->sMemBlk.hOSMemHandle,
			ui32ROffOffset,
			pui8LinAddr,
			&ui32PageOffset);

	
	sDevVPageAddr.uiAddr = sDevVAddr.uiAddr - ui32PageOffset;

	PVR_ASSERT((sDevVPageAddr.uiAddr & 0xFFF) == 0);

	
	BM_GetPhysPageAddr(psROffMemInfo, sDevVPageAddr, &sDevPAddr);

	
	sDevPAddr.uiAddr += ui32PageOffset;

	eErr = PDumpOSBufprintf(hScript,
			 ui32MaxLen,
			 "CBP :SGXMEM:PA_%8.8lX%8.8lX:0x%8.8lX 0x%8.8lX 0x%8.8lX 0x%8.8lX\r\n",
			 (IMG_UINT32) hUniqueTag,
			 sDevPAddr.uiAddr & ~(SGX_MMU_PAGE_MASK),
			 sDevPAddr.uiAddr & (SGX_MMU_PAGE_MASK),
			 ui32WPosVal,
			 ui32PacketSize,
			 ui32BufferSize);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);
	return PVRSRV_OK;
}


PVRSRV_ERROR PDumpIDLWithFlags(IMG_UINT32 ui32Clocks, IMG_UINT32 ui32Flags)
{
	PVRSRV_ERROR eErr;
	PDUMP_GET_SCRIPT_STRING();
	PDUMP_DBG(("PDumpIDLWithFlags"));

	eErr = PDumpOSBufprintf(hScript, ui32MaxLen, "IDL %lu\r\n", ui32Clocks);
	if(eErr != PVRSRV_OK)
	{
		return eErr;
	}
	PDumpOSWriteString2(hScript, ui32Flags);
	return PVRSRV_OK;
}


PVRSRV_ERROR PDumpIDL(IMG_UINT32 ui32Clocks)
{
	return PDumpIDLWithFlags(ui32Clocks, PDUMP_FLAGS_CONTINUOUS);
}
#endif 


PVRSRV_ERROR PDumpMemUM(PVRSRV_PER_PROCESS_DATA *psPerProc,
						IMG_PVOID pvAltLinAddrUM,
						IMG_PVOID pvLinAddrUM,
						PVRSRV_KERNEL_MEM_INFO *psMemInfo,
						IMG_UINT32 ui32Offset,
						IMG_UINT32 ui32Bytes,
						IMG_UINT32 ui32Flags,
						IMG_HANDLE hUniqueTag)
{
	IMG_VOID *pvAddrUM;
	IMG_VOID *pvAddrKM;
	IMG_UINT32 ui32BytesDumped;
	IMG_UINT32 ui32CurrentOffset;

	if (psMemInfo->pvLinAddrKM != IMG_NULL && pvAltLinAddrUM == IMG_NULL)
	{
		
		return PDumpMemKM(IMG_NULL,
					   psMemInfo,
					   ui32Offset,
					   ui32Bytes,
					   ui32Flags,
					   hUniqueTag);
	}

	pvAddrUM = (pvAltLinAddrUM != IMG_NULL) ? pvAltLinAddrUM : ((pvLinAddrUM != IMG_NULL) ? VPTR_PLUS(pvLinAddrUM, ui32Offset) : IMG_NULL);

	pvAddrKM = GetTempBuffer();

	
	PVR_ASSERT(pvAddrUM != IMG_NULL && pvAddrKM != IMG_NULL);
	if (pvAddrUM == IMG_NULL || pvAddrKM == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpMemUM: Nothing to dump"));
		return PVRSRV_ERROR_GENERIC;
	}

	if (ui32Bytes > PDUMP_TEMP_BUFFER_SIZE)
	{
		PDumpCommentWithFlags(ui32Flags, "Dumping 0x%8.8lx bytes of memory, in blocks of 0x%8.8lx bytes", ui32Bytes, (IMG_UINT32)PDUMP_TEMP_BUFFER_SIZE);
	}

	ui32CurrentOffset = ui32Offset;
	for (ui32BytesDumped = 0; ui32BytesDumped < ui32Bytes;)
	{
		PVRSRV_ERROR eError;
		IMG_UINT32 ui32BytesToDump = MIN(PDUMP_TEMP_BUFFER_SIZE, ui32Bytes - ui32BytesDumped);

		eError = OSCopyFromUser(psPerProc,
					   pvAddrKM,
					   pvAddrUM,
					   ui32BytesToDump);
		if (eError != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "PDumpMemUM: OSCopyFromUser failed (%d), eError"));
			return PVRSRV_ERROR_GENERIC;
		}

		eError = PDumpMemKM(pvAddrKM,
					   psMemInfo,
					   ui32CurrentOffset,
					   ui32BytesToDump,
					   ui32Flags,
					   hUniqueTag);

		if (eError != PVRSRV_OK)
		{
			
			if (ui32BytesDumped != 0)
			{
				PVR_DPF((PVR_DBG_ERROR, "PDumpMemUM: PDumpMemKM failed (%d)", eError));
			}
			PVR_ASSERT(ui32BytesDumped == 0);
			return eError;
		}

		VPTR_INC(pvAddrUM, ui32BytesToDump);
		ui32CurrentOffset += ui32BytesToDump;
		ui32BytesDumped += ui32BytesToDump;
	}

	return PVRSRV_OK;
}


static PVRSRV_ERROR _PdumpAllocMMUContext(IMG_UINT32 *pui32MMUContextID)
{
	IMG_UINT32 i;

	
	for(i=0; i<MAX_PDUMP_MMU_CONTEXTS; i++)
	{
		if((gui16MMUContextUsage & (1U << i)) == 0)
		{
			
			gui16MMUContextUsage |= 1U << i;
			*pui32MMUContextID = i;
			return PVRSRV_OK;
		}
	}

	PVR_DPF((PVR_DBG_ERROR, "_PdumpAllocMMUContext: no free MMU context ids"));

	return PVRSRV_ERROR_GENERIC;
}


static PVRSRV_ERROR _PdumpFreeMMUContext(IMG_UINT32 ui32MMUContextID)
{
	if(ui32MMUContextID < MAX_PDUMP_MMU_CONTEXTS)
	{
		
		gui16MMUContextUsage &= ~(1U << ui32MMUContextID);
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_ERROR, "_PdumpFreeMMUContext: MMU context ids invalid"));

	return PVRSRV_ERROR_GENERIC;
}


PVRSRV_ERROR PDumpSetMMUContext(PVRSRV_DEVICE_TYPE eDeviceType,
								IMG_CHAR *pszMemSpace,
								IMG_UINT32 *pui32MMUContextID,
								IMG_UINT32 ui32MMUType,
								IMG_HANDLE hUniqueTag1,
								IMG_VOID *pvPDCPUAddr)
{
	IMG_UINT8 *pui8LinAddr = (IMG_UINT8 *)pvPDCPUAddr;
	IMG_CPU_PHYADDR sCpuPAddr;
	IMG_DEV_PHYADDR sDevPAddr;
	IMG_UINT32 ui32MMUContextID;
	PVRSRV_ERROR eError;

	eError = _PdumpAllocMMUContext(&ui32MMUContextID);
	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpSetMMUContext: _PdumpAllocMMUContext failed: %d", eError));
		return eError;
	}

	
	sCpuPAddr = OSMapLinToCPUPhys(pui8LinAddr);
	sDevPAddr = SysCpuPAddrToDevPAddr(eDeviceType, sCpuPAddr);
	
	sDevPAddr.uiAddr &= ~((PVRSRV_4K_PAGE_SIZE) -1);

	PDumpComment("Set MMU Context\r\n");
	
	PDumpComment("MMU :%s:v%d %d :%s:PA_%8.8lX%8.8lX\r\n",
						pszMemSpace,
						ui32MMUContextID,
						ui32MMUType,
						pszMemSpace,
						hUniqueTag1,
						sDevPAddr.uiAddr);

	
	*pui32MMUContextID = ui32MMUContextID;

	return PVRSRV_OK;
}


PVRSRV_ERROR PDumpClearMMUContext(PVRSRV_DEVICE_TYPE eDeviceType,
								IMG_CHAR *pszMemSpace,
								IMG_UINT32 ui32MMUContextID,
								IMG_UINT32 ui32MMUType)
{
	PVRSRV_ERROR eError;

	PVR_UNREFERENCED_PARAMETER(eDeviceType);

	
	PDumpComment("Clear MMU Context for memory space %s\r\n", pszMemSpace);
	
	PDumpComment("MMU :%s:v%d %d\r\n",
						pszMemSpace,
						ui32MMUContextID,
						ui32MMUType);

	eError = _PdumpFreeMMUContext(ui32MMUContextID);
	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PDumpClearMMUContext: _PdumpFreeMMUContext failed: %d", eError));
		return eError;
	}

	return PVRSRV_OK;
}

#else	
#endif	
