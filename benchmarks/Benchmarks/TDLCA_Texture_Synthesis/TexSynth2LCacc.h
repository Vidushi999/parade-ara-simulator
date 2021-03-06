#ifndef LCACC_HEADER_SECTION
#define LCACC_HEADER_SECTION
#include "SimicsHeader.h"
#include <stdint.h>
#include <cassert>
#include <vector>
#define LCACC_STATUS_TLB_MISS 45
#define LCACC_STATUS_COMPLETED 46
#define LCACC_STATUS_ERROR 3
#define LCACC_STATUS_PENDING 4
#define LCACC_GAM_WAIT 5
#define LCACC_GAM_GRANT 4
#define LCACC_GAM_ERROR 6
#define LCACC_GAM_REVOKE 7
#define LCACC_CMD_BEGIN_TASK 42
#define LCACC_CMD_CANCEL_TASK 43
#define LCACC_CMD_TLB_SERVICE 44
#define LCACC_CMD_BEGIN_TASK_SIGNATURE 47
#define LCACC_CMD_BEGIN_PROGRAM 50
#define BIN_CMD_ARBITRATE_RESPONSE 102
#define LWI_WAIT 2
#define LWI_PROCEED 1
#define LWI_ERROR 0
template <class From, class To>
class TypeConverter
{
public:
	union
	{
		From from;
		To to;
	};
};
template <class From, class To>
inline To ConvertToType(From f)
{
	TypeConverter<From, To> x;
	x.from = f;
	return x.to;
}
template <class From>
class ByteConverter
{
public:
	union
	{
		From from;
		uint8_t bytes[sizeof(From)];
	};
};
template <class From>
inline void ConvertToBytes(std::vector<uint8_t>& dst, size_t index, From f)
{
	ByteConverter<From> c;
	c.from = f;
	for(size_t i = 0; i < sizeof(From); i++)
	{
		assert(i + index < dst.size());
		dst[i + index] = c.bytes[i];
	}
}
class LCAccNode
{
	std::vector<std::vector<uint8_t>*> lcaccIDReplacementBuffer;
	std::vector<size_t> lcaccIDReplacementIndex;
	bool lcaccIDDecided;
public:
	uint32_t lcaccOpCode;
	uint32_t spmWindowSize;
	uint32_t spmWindowCount;
	uint32_t spmWindowOffset;
	uint32_t lcaccID;
	inline LCAccNode(uint32_t init_lcaccOpCode)
	{
		lcaccOpCode = init_lcaccOpCode;
		spmWindowSize = 0;
		spmWindowCount = 0;
		spmWindowOffset = 0;
		lcaccIDDecided = false;
		lcaccID = 0;
	}
	inline LCAccNode(uint32_t init_lcaccOpCode, uint32_t init_spmWindowSize, uint32_t init_spmWindowCount, uint32_t init_spmWindowOffset)
	{
		lcaccOpCode = init_lcaccOpCode;
		spmWindowSize = init_spmWindowSize;
		spmWindowCount = init_spmWindowCount;
		spmWindowOffset = init_spmWindowOffset;
		lcaccIDDecided = false;
		lcaccID = 0;
	}
	inline LCAccNode(uint32_t init_lcaccOpCode, uint32_t init_spmWindowSize, uint32_t init_spmWindowCount, uint32_t init_spmWindowOffset, uint32_t init_lcaccID)
	{
		lcaccOpCode = init_lcaccOpCode;
		spmWindowSize = init_spmWindowSize;
		spmWindowCount = init_spmWindowCount;
		spmWindowOffset = init_spmWindowOffset;
		lcaccIDDecided = true;
		lcaccID = init_lcaccID;
	}
	inline void SetSPMConfig(uint32_t init_spmWindowSize, uint32_t init_spmWindowCount, uint32_t init_spmWindowOffset)
	{
		spmWindowSize = init_spmWindowSize;
		spmWindowCount = init_spmWindowCount;
		spmWindowOffset = init_spmWindowOffset;
	}
	inline void PlaceLCAccID(std::vector<uint8_t>& buffer, size_t index)
	{
		if(lcaccIDDecided)
		{
			ConvertToBytes<uint32_t>(buffer, index, lcaccID);
		}
		lcaccIDReplacementBuffer.push_back(&buffer);
		lcaccIDReplacementIndex.push_back(index);
	}
	inline void SetLCAccID(uint32_t init_lcaccID)
	{
		simics_assert(!lcaccIDDecided);
		lcaccIDDecided = true;
		lcaccID = init_lcaccID;
		for(size_t i = 0; i < lcaccIDReplacementBuffer.size(); i++)
		{
			ConvertToBytes<uint32_t>(*lcaccIDReplacementBuffer[i], lcaccIDReplacementIndex[i], lcaccID);
		}
	}
	inline void Reset()
	{
		lcaccIDDecided = false;
	}
};
class MicroprogramWriter
{
	int16_t computesHaveBeenWritten;
	int16_t transfersHaveBeenWritten;
	bool finalized;
	std::vector<uint8_t> buffer;
	bool signature;
	uint32_t taskGrain;
public:
	class ComputeArgIndex
	{
	public:
		uint32_t baseAddr;
		uint32_t elementSize;
		std::vector<uint32_t> size;
		std::vector<int32_t> stride;
		inline ComputeArgIndex(uint32_t init_baseAddr, uint32_t init_elementSize, const std::vector<uint32_t>& init_size, const std::vector<int32_t>& init_stride)
		{
			baseAddr = init_baseAddr;
			elementSize = init_elementSize;
			size = init_size;
			stride = init_stride;
		}
	};
	inline MicroprogramWriter(bool init_signature)
	{
		finalized = false;
		computesHaveBeenWritten = 0;
		transfersHaveBeenWritten = 0;
		taskGrain = 0;
		signature = init_signature;
		if(signature)
		{
			for(int i = 0; i < sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t); i++)
			{
				buffer.push_back(0);
			}
		}
		else
		{
			for(int i = 0; i < sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t); i++)
			{
				buffer.push_back(0);
			}
		}
	}
	inline void SetTaskGrain(uint32_t tg)
	{
		simics_assert(!finalized);
		simics_assert(!signature);
		taskGrain = tg;
	}
	inline void AddTransfer(LCAccNode& src, uintptr_t srcBaseAddr, const std::vector<uint32_t>& srcSize, const std::vector<int32_t>& srcStride, void* dst, const std::vector<uint32_t>& blockSize, const std::vector<int32_t>& blockStride, const std::vector<uint32_t>& elementSize, const std::vector<int32_t>& elementStride, uint32_t atomSize)
	{
		simics_assert(!finalized);
		simics_assert(computesHaveBeenWritten);
		simics_assert(srcSize.size() == srcStride.size());
		simics_assert(blockSize.size() == blockStride.size());
		simics_assert(elementSize.size() == elementStride.size());
		transfersHaveBeenWritten++;
		int chunkSize = sizeof(uintptr_t) * 2 + sizeof(uint32_t) * (3 + srcSize.size() + blockSize.size() + elementSize.size()) + sizeof(int32_t) * (1 + srcSize.size() + blockSize.size() + elementSize.size()) + sizeof(uint8_t) * 5;
		size_t chunkPosition = buffer.size();
		for(int i = 0; i < chunkSize; i++)
		{
			buffer.push_back(0);
		}
		src.PlaceLCAccID(buffer, chunkPosition); chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uintptr_t>(buffer, chunkPosition, (uintptr_t)(srcBaseAddr));  chunkPosition += sizeof(uintptr_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(1));  chunkPosition += sizeof(uint8_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(srcSize.size()));  chunkPosition += sizeof(uint8_t);
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(-1));  chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uintptr_t>(buffer, chunkPosition, (uintptr_t)(dst));  chunkPosition += sizeof(uintptr_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(blockSize.size()));  chunkPosition += sizeof(uint8_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(elementSize.size()));  chunkPosition += sizeof(uint8_t);
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(src.spmWindowCount));  chunkPosition += sizeof(uint32_t);//first, position the element in the spm
		ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(src.spmWindowSize));  chunkPosition += sizeof(int32_t);
		for(size_t i = 0; i < srcSize.size(); i++)
		{
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(srcSize[i]));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(srcStride[i]));  chunkPosition += sizeof(int32_t);
		}
		for(size_t i = 0; i < blockSize.size(); i++)
		{
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(blockSize[i]));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(blockStride[i]));  chunkPosition += sizeof(int32_t);
		}
		for(size_t i = 0; i < elementSize.size(); i++)
		{
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(elementSize[i]));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(elementStride[i]));  chunkPosition += sizeof(int32_t);
		}
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(atomSize));  chunkPosition += sizeof(uint8_t);
	}
	inline void AddTransfer(const void* src, const std::vector<uint32_t>& blockSize, const std::vector<int32_t>& blockStride, const std::vector<uint32_t>& elementSize, const std::vector<int32_t>& elementStride, LCAccNode& dst, uintptr_t dstBaseAddr, const std::vector<uint32_t>& dstSize, const std::vector<int32_t>& dstStride, uint32_t atomSize)
	{
		simics_assert(!finalized);
		simics_assert(computesHaveBeenWritten);
		simics_assert(dstSize.size() == dstStride.size());
		simics_assert(blockSize.size() == blockStride.size());
		simics_assert(elementSize.size() == elementStride.size());
		transfersHaveBeenWritten++;
		int chunkSize = sizeof(uintptr_t) * 2 + sizeof(uint32_t) * (3 + dstSize.size() + blockSize.size() + elementSize.size()) + sizeof(int32_t) * (1 + dstSize.size() + blockSize.size() + elementSize.size()) + sizeof(uint8_t) * 5;
		size_t chunkPosition = buffer.size();
		for(int i = 0; i < chunkSize; i++)
		{
			buffer.push_back(0);
		}
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(-1));  chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uintptr_t>(buffer, chunkPosition, (uintptr_t)(src));  chunkPosition += sizeof(uintptr_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(blockSize.size()));  chunkPosition += sizeof(uint8_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(elementSize.size()));  chunkPosition += sizeof(uint8_t);
		dst.PlaceLCAccID(buffer, chunkPosition); chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uintptr_t>(buffer, chunkPosition, (uintptr_t)(dstBaseAddr));  chunkPosition += sizeof(uintptr_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(1));  chunkPosition += sizeof(uint8_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(dstSize.size()));  chunkPosition += sizeof(uint8_t);
		for(size_t i = 0; i < blockSize.size(); i++)
		{
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(blockSize[i]));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(blockStride[i]));  chunkPosition += sizeof(int32_t);
		}
		for(size_t i = 0; i < elementSize.size(); i++)
		{
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(elementSize[i]));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(elementStride[i]));  chunkPosition += sizeof(int32_t);
		}
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(dst.spmWindowCount));  chunkPosition += sizeof(uint32_t);//first, position the element in the spm
		ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(dst.spmWindowSize));  chunkPosition += sizeof(int32_t);
		for(size_t i = 0; i < dstSize.size(); i++)
		{
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(dstSize[i]));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(dstStride[i]));  chunkPosition += sizeof(int32_t);
		}
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(atomSize));  chunkPosition += sizeof(uint8_t);
	}
	inline void AddTransfer(LCAccNode& src, uint32_t srcBaseAddr, const std::vector<uint32_t>& srcSize, const std::vector<int32_t>& srcStride, LCAccNode& dst, uint32_t dstBaseAddr, const std::vector<uint32_t>& dstSize, const std::vector<int32_t>& dstStride, uint32_t atomSize)
	{
		simics_assert(!finalized);
		simics_assert(computesHaveBeenWritten);
		simics_assert(dstSize.size() == dstStride.size());
		simics_assert(srcSize.size() == srcStride.size());
		transfersHaveBeenWritten++;
		int chunkSize = sizeof(uintptr_t) * 2 + sizeof(uint32_t) * (4 + srcSize.size() + dstSize.size()) + sizeof(int32_t) * (2 + srcSize.size() + dstSize.size()) + sizeof(uint8_t) * 5;
		size_t chunkPosition = buffer.size();
		for(int i = 0; i < chunkSize; i++)
		{
			buffer.push_back(0);
		}
		src.PlaceLCAccID(buffer, chunkPosition); chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uintptr_t>(buffer, chunkPosition, (uintptr_t)(srcBaseAddr));  chunkPosition += sizeof(uintptr_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(1));  chunkPosition += sizeof(uint8_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(srcSize.size()));  chunkPosition += sizeof(uint8_t);
		dst.PlaceLCAccID(buffer, chunkPosition); chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uintptr_t>(buffer, chunkPosition, (uintptr_t)(dstBaseAddr));  chunkPosition += sizeof(uintptr_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(1));  chunkPosition += sizeof(uint8_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(dstSize.size()));  chunkPosition += sizeof(uint8_t);
		//Source first
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(src.spmWindowCount));  chunkPosition += sizeof(uint32_t);//first, position the element in the spm
		ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(src.spmWindowSize));  chunkPosition += sizeof(int32_t);
		for(size_t i = 0; i < srcSize.size(); i++)
		{
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(srcSize[i]));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(srcStride[i]));  chunkPosition += sizeof(int32_t);
		}
		//Destination next
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(dst.spmWindowCount));  chunkPosition += sizeof(uint32_t);//first, position the element in the spm
		ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(dst.spmWindowSize));  chunkPosition += sizeof(int32_t);
		for(size_t i = 0; i < dstSize.size(); i++)
		{
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(dstSize[i]));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(dstStride[i]));  chunkPosition += sizeof(int32_t);
		}
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(atomSize));  chunkPosition += sizeof(uint8_t);
	}
	inline void AddCompute(LCAccNode& node, const std::vector<ComputeArgIndex>& indexSet, const std::vector<uint64_t>& registers)
	{
		simics_assert(!finalized);
		simics_assert(!transfersHaveBeenWritten);
		computesHaveBeenWritten++;
		int chunkSize = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint8_t) + (sizeof(uint64_t) * registers.size());
		for(size_t i = 0; i < indexSet.size(); i++)
		{
			chunkSize += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) + ((sizeof(int32_t) * indexSet[i].stride.size()) + (sizeof(uint32_t) * indexSet[i].size.size()));
		}
		size_t chunkPosition = buffer.size();
		for(int i = 0; i < chunkSize; i++)
		{
			buffer.push_back(0);
		}
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(node.lcaccOpCode));  chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(node.spmWindowCount));  chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(node.spmWindowSize));  chunkPosition += sizeof(uint32_t);
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(indexSet.size()));  chunkPosition += sizeof(uint8_t);
		for(size_t i = 0; i < indexSet.size(); i++)
		{
			simics_assert(indexSet[i].size.size() == indexSet[i].stride.size());
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(indexSet[i].baseAddr));  chunkPosition += sizeof(uint32_t);
			ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(indexSet[i].size.size()));  chunkPosition += sizeof(uint8_t);
			for(size_t j = 0; j < indexSet[i].size.size(); j++)
			{
				ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(indexSet[i].size[j]));  chunkPosition += sizeof(uint32_t);
				ConvertToBytes<int32_t>(buffer, chunkPosition, (int32_t)(indexSet[i].stride[j]));  chunkPosition += sizeof(int32_t);
			}
			ConvertToBytes<uint32_t>(buffer, chunkPosition, (uint32_t)(indexSet[i].elementSize));  chunkPosition += sizeof(uint32_t);
		}
		ConvertToBytes<uint8_t>(buffer, chunkPosition, (uint8_t)(registers.size()));  chunkPosition += sizeof(uint8_t);
		for(size_t i = 0; i < registers.size(); i++)
		{
			ConvertToBytes<uint64_t>(buffer, chunkPosition, (uint64_t)(registers[i]));  chunkPosition += sizeof(uint64_t);
		}
	}
	inline void Finalize(uint32_t skipTasks, uint32_t numberOfTasks)
	{
		simics_assert(!finalized);
		if(signature)
		{
			ConvertToBytes<uint8_t>(buffer, 0, transfersHaveBeenWritten);
			ConvertToBytes<uint8_t>(buffer, sizeof(uint8_t), computesHaveBeenWritten);
			ConvertToBytes<uint32_t>(buffer, sizeof(uint8_t) + sizeof(uint8_t), skipTasks);
			ConvertToBytes<uint32_t>(buffer, sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint32_t), numberOfTasks);
		}
		else
		{
			simics_assert(skipTasks == 0);
			ConvertToBytes<uint16_t>(buffer, 0, computesHaveBeenWritten);
			ConvertToBytes<uint16_t>(buffer, sizeof(uint16_t), transfersHaveBeenWritten);
			ConvertToBytes<uint32_t>(buffer, sizeof(uint16_t) + sizeof(uint16_t), taskGrain);
			ConvertToBytes<uint32_t>(buffer, sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint32_t), numberOfTasks);
		}
		finalized = true;
	}
	inline void Finalize(uint32_t numberOfTasks)
	{
		Finalize(0, numberOfTasks);
	}
	inline uint8_t* GetBuffer()
	{
		simics_assert(finalized);
		return &(buffer.at(0));
	}
	inline uint32_t GetBufferSize() const
	{
		simics_assert(finalized);
		return (uint32_t)(buffer.size());
	}
	inline bool IsFinalized() const
	{
		return finalized;
	}
};
inline void PrepareAsync_td(int thread, InterruptArgs& isrArgs)
{
	isrArgs.threadID = thread;
	isrArgs.lcaccID = 0;
	isrArgs.lcaccMode = 0;
	LWI_RegisterInterruptHandler(&isrArgs);
}
inline void FireAsync_td_buf(uint8_t* buf, uint32_t bufSize, int thread)
{
	LCAcc_Command(thread, 0, LCACC_CMD_BEGIN_PROGRAM, buf, bufSize, 0, 0);
}
inline void WaitAsync_td(int thread, InterruptArgs& isrArgs, int count)
{
	for(int i = 0; i < count; i++)
	{
		bool stillWorking = true;
		while(stillWorking)
		{
			InterruptArgs* args = 0;
			while((args = LWI_CheckInterrupt(thread)) == 0);
			simics_assert(args->lcaccMode == 0);
			switch(args->status)
			{
				case(LCACC_STATUS_TLB_MISS):
					LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
					break;
				case(LCACC_STATUS_COMPLETED):
					stillWorking = false;
					break;
				default:
					simics_assert(0);
					stillWorking = false;
			}
			LWI_ClearInterrupt(thread);
		}
	}
}

#endif
#ifndef LCACC_BODY_SIG__TexSynth2LCacc__X
#define LCACC_BODY_SIG__TexSynth2LCacc__X
#define LCACC_CLASS_SIG__TexSynth2LCacc__atlasReader 1
#define LCACC_CLASS_SIG__TexSynth2LCacc__targetReader0 2
#define LCACC_CLASS_SIG__TexSynth2LCacc__resultReader0 3
#define LCACC_CLASS_SIG__TexSynth2LCacc__regionSample0 4
#define LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc0 5
#define LCACC_CLASS_SIG__TexSynth2LCacc__targetReader1 6
#define LCACC_CLASS_SIG__TexSynth2LCacc__resultReader1 7
#define LCACC_CLASS_SIG__TexSynth2LCacc__regionSample1 8
#define LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc1 9
#define LCACC_CLASS_SIG__TexSynth2LCacc__targetReader2 10
#define LCACC_CLASS_SIG__TexSynth2LCacc__resultReader2 11
#define LCACC_CLASS_SIG__TexSynth2LCacc__regionSample2 12
#define LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc2 13
#define LCACC_CLASS_SIG__TexSynth2LCacc__targetReader3 14
#define LCACC_CLASS_SIG__TexSynth2LCacc__resultReader3 15
#define LCACC_CLASS_SIG__TexSynth2LCacc__regionSample3 16
#define LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc3 17
#define LCACC_CLASS_SIG__TexSynth2LCacc__pickCandidate 18
class InstanceData_sig__TexSynth2LCacc
{
public:
	InterruptArgs GAM_INTERACTION;
	int threadID;
	uint32_t binBufSize;
	unsigned int pendingAccelerators;
	unsigned int reservedAccelerators;
	unsigned int acceleratorVectorLength;
	int allocatedAcceleratorCount__TexSynth2;
	int allocatedAcceleratorIDSet__TexSynth2[9];
	int allocatedAcceleratorCount__TexSynth3;
	int allocatedAcceleratorIDSet__TexSynth3[4];
	int allocatedAcceleratorCount__TexSynth4;
	int allocatedAcceleratorIDSet__TexSynth4[4];
	int allocatedAcceleratorCount__TexSynth5;
	int allocatedAcceleratorIDSet__TexSynth5[1];
	float (*LCAcc_FuncArgs__resultImage);
	int LCAcc_FuncVars__inputHeight;
	int LCAcc_FuncVars__inputWidth;
	int LCAcc_FuncVars__outputHeight;
	int LCAcc_FuncVars__outputWidth;
	int LCAcc_FuncVars__imageCount;
	intptr_t LCAcc_FuncVars__imageArrayStart;
	intptr_t LCAcc_FuncVars__targetArrayStart;
	intptr_t LCAcc_FuncVars__atlasArray;
	uint32_t LCAcc_FuncVars__lineNumber;
	int LCAcc_FuncVars__chunk;
	MicroprogramWriter acceleratorSignature__atlasReader;
	LCAccNode node_atlasReader;
	InterruptArgs HandlerArgs__atlasReader;
	MicroprogramWriter acceleratorSignature__targetReader0;
	LCAccNode node_targetReader0;
	InterruptArgs HandlerArgs__targetReader0;
	MicroprogramWriter acceleratorSignature__resultReader0;
	LCAccNode node_resultReader0;
	InterruptArgs HandlerArgs__resultReader0;
	MicroprogramWriter acceleratorSignature__regionSample0;
	LCAccNode node_regionSample0;
	InterruptArgs HandlerArgs__regionSample0;
	MicroprogramWriter acceleratorSignature__diffCalc0;
	LCAccNode node_diffCalc0;
	InterruptArgs HandlerArgs__diffCalc0;
	MicroprogramWriter acceleratorSignature__targetReader1;
	LCAccNode node_targetReader1;
	InterruptArgs HandlerArgs__targetReader1;
	MicroprogramWriter acceleratorSignature__resultReader1;
	LCAccNode node_resultReader1;
	InterruptArgs HandlerArgs__resultReader1;
	MicroprogramWriter acceleratorSignature__regionSample1;
	LCAccNode node_regionSample1;
	InterruptArgs HandlerArgs__regionSample1;
	MicroprogramWriter acceleratorSignature__diffCalc1;
	LCAccNode node_diffCalc1;
	InterruptArgs HandlerArgs__diffCalc1;
	MicroprogramWriter acceleratorSignature__targetReader2;
	LCAccNode node_targetReader2;
	InterruptArgs HandlerArgs__targetReader2;
	MicroprogramWriter acceleratorSignature__resultReader2;
	LCAccNode node_resultReader2;
	InterruptArgs HandlerArgs__resultReader2;
	MicroprogramWriter acceleratorSignature__regionSample2;
	LCAccNode node_regionSample2;
	InterruptArgs HandlerArgs__regionSample2;
	MicroprogramWriter acceleratorSignature__diffCalc2;
	LCAccNode node_diffCalc2;
	InterruptArgs HandlerArgs__diffCalc2;
	MicroprogramWriter acceleratorSignature__targetReader3;
	LCAccNode node_targetReader3;
	InterruptArgs HandlerArgs__targetReader3;
	MicroprogramWriter acceleratorSignature__resultReader3;
	LCAccNode node_resultReader3;
	InterruptArgs HandlerArgs__resultReader3;
	MicroprogramWriter acceleratorSignature__regionSample3;
	LCAccNode node_regionSample3;
	InterruptArgs HandlerArgs__regionSample3;
	MicroprogramWriter acceleratorSignature__diffCalc3;
	LCAccNode node_diffCalc3;
	InterruptArgs HandlerArgs__diffCalc3;
	MicroprogramWriter acceleratorSignature__pickCandidate;
	LCAccNode node_pickCandidate;
	InterruptArgs HandlerArgs__pickCandidate;
	inline void Reset()
	{
		pendingAccelerators = 0;
		allocatedAcceleratorCount__TexSynth2 = 0;
		allocatedAcceleratorCount__TexSynth3 = 0;
		allocatedAcceleratorCount__TexSynth4 = 0;
		allocatedAcceleratorCount__TexSynth5 = 0;
		reservedAccelerators = 0;
		allocatedAcceleratorIDSet__TexSynth2[0] = 0;
		allocatedAcceleratorIDSet__TexSynth2[1] = 0;
		allocatedAcceleratorIDSet__TexSynth2[2] = 0;
		allocatedAcceleratorIDSet__TexSynth2[3] = 0;
		allocatedAcceleratorIDSet__TexSynth2[4] = 0;
		allocatedAcceleratorIDSet__TexSynth2[5] = 0;
		allocatedAcceleratorIDSet__TexSynth2[6] = 0;
		allocatedAcceleratorIDSet__TexSynth2[7] = 0;
		allocatedAcceleratorIDSet__TexSynth2[8] = 0;
		allocatedAcceleratorIDSet__TexSynth3[0] = 0;
		allocatedAcceleratorIDSet__TexSynth3[1] = 0;
		allocatedAcceleratorIDSet__TexSynth3[2] = 0;
		allocatedAcceleratorIDSet__TexSynth3[3] = 0;
		allocatedAcceleratorIDSet__TexSynth4[0] = 0;
		allocatedAcceleratorIDSet__TexSynth4[1] = 0;
		allocatedAcceleratorIDSet__TexSynth4[2] = 0;
		allocatedAcceleratorIDSet__TexSynth4[3] = 0;
		allocatedAcceleratorIDSet__TexSynth5[0] = 0;
		HandlerArgs__atlasReader.threadID = threadID;
		HandlerArgs__atlasReader.status = 0;
		HandlerArgs__atlasReader.taskIndex = 0;
		HandlerArgs__atlasReader.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__atlasReader;
		node_atlasReader.Reset();
		HandlerArgs__targetReader0.threadID = threadID;
		HandlerArgs__targetReader0.status = 0;
		HandlerArgs__targetReader0.taskIndex = 0;
		HandlerArgs__targetReader0.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__targetReader0;
		node_targetReader0.Reset();
		HandlerArgs__resultReader0.threadID = threadID;
		HandlerArgs__resultReader0.status = 0;
		HandlerArgs__resultReader0.taskIndex = 0;
		HandlerArgs__resultReader0.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__resultReader0;
		node_resultReader0.Reset();
		HandlerArgs__regionSample0.threadID = threadID;
		HandlerArgs__regionSample0.status = 0;
		HandlerArgs__regionSample0.taskIndex = 0;
		HandlerArgs__regionSample0.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__regionSample0;
		node_regionSample0.Reset();
		HandlerArgs__diffCalc0.threadID = threadID;
		HandlerArgs__diffCalc0.status = 0;
		HandlerArgs__diffCalc0.taskIndex = 0;
		HandlerArgs__diffCalc0.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc0;
		node_diffCalc0.Reset();
		HandlerArgs__targetReader1.threadID = threadID;
		HandlerArgs__targetReader1.status = 0;
		HandlerArgs__targetReader1.taskIndex = 0;
		HandlerArgs__targetReader1.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__targetReader1;
		node_targetReader1.Reset();
		HandlerArgs__resultReader1.threadID = threadID;
		HandlerArgs__resultReader1.status = 0;
		HandlerArgs__resultReader1.taskIndex = 0;
		HandlerArgs__resultReader1.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__resultReader1;
		node_resultReader1.Reset();
		HandlerArgs__regionSample1.threadID = threadID;
		HandlerArgs__regionSample1.status = 0;
		HandlerArgs__regionSample1.taskIndex = 0;
		HandlerArgs__regionSample1.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__regionSample1;
		node_regionSample1.Reset();
		HandlerArgs__diffCalc1.threadID = threadID;
		HandlerArgs__diffCalc1.status = 0;
		HandlerArgs__diffCalc1.taskIndex = 0;
		HandlerArgs__diffCalc1.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc1;
		node_diffCalc1.Reset();
		HandlerArgs__targetReader2.threadID = threadID;
		HandlerArgs__targetReader2.status = 0;
		HandlerArgs__targetReader2.taskIndex = 0;
		HandlerArgs__targetReader2.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__targetReader2;
		node_targetReader2.Reset();
		HandlerArgs__resultReader2.threadID = threadID;
		HandlerArgs__resultReader2.status = 0;
		HandlerArgs__resultReader2.taskIndex = 0;
		HandlerArgs__resultReader2.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__resultReader2;
		node_resultReader2.Reset();
		HandlerArgs__regionSample2.threadID = threadID;
		HandlerArgs__regionSample2.status = 0;
		HandlerArgs__regionSample2.taskIndex = 0;
		HandlerArgs__regionSample2.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__regionSample2;
		node_regionSample2.Reset();
		HandlerArgs__diffCalc2.threadID = threadID;
		HandlerArgs__diffCalc2.status = 0;
		HandlerArgs__diffCalc2.taskIndex = 0;
		HandlerArgs__diffCalc2.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc2;
		node_diffCalc2.Reset();
		HandlerArgs__targetReader3.threadID = threadID;
		HandlerArgs__targetReader3.status = 0;
		HandlerArgs__targetReader3.taskIndex = 0;
		HandlerArgs__targetReader3.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__targetReader3;
		node_targetReader3.Reset();
		HandlerArgs__resultReader3.threadID = threadID;
		HandlerArgs__resultReader3.status = 0;
		HandlerArgs__resultReader3.taskIndex = 0;
		HandlerArgs__resultReader3.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__resultReader3;
		node_resultReader3.Reset();
		HandlerArgs__regionSample3.threadID = threadID;
		HandlerArgs__regionSample3.status = 0;
		HandlerArgs__regionSample3.taskIndex = 0;
		HandlerArgs__regionSample3.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__regionSample3;
		node_regionSample3.Reset();
		HandlerArgs__diffCalc3.threadID = threadID;
		HandlerArgs__diffCalc3.status = 0;
		HandlerArgs__diffCalc3.taskIndex = 0;
		HandlerArgs__diffCalc3.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc3;
		node_diffCalc3.Reset();
		HandlerArgs__pickCandidate.threadID = threadID;
		HandlerArgs__pickCandidate.status = 0;
		HandlerArgs__pickCandidate.taskIndex = 0;
		HandlerArgs__pickCandidate.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__pickCandidate;
		node_pickCandidate.Reset();
	}
	inline InstanceData_sig__TexSynth2LCacc() :
		acceleratorSignature__atlasReader(true), 
		node_atlasReader(911), 
		acceleratorSignature__targetReader0(true), 
		node_targetReader0(911), 
		acceleratorSignature__resultReader0(true), 
		node_resultReader0(911), 
		acceleratorSignature__regionSample0(true), 
		node_regionSample0(912), 
		acceleratorSignature__diffCalc0(true), 
		node_diffCalc0(913), 
		acceleratorSignature__targetReader1(true), 
		node_targetReader1(911), 
		acceleratorSignature__resultReader1(true), 
		node_resultReader1(911), 
		acceleratorSignature__regionSample1(true), 
		node_regionSample1(912), 
		acceleratorSignature__diffCalc1(true), 
		node_diffCalc1(913), 
		acceleratorSignature__targetReader2(true), 
		node_targetReader2(911), 
		acceleratorSignature__resultReader2(true), 
		node_resultReader2(911), 
		acceleratorSignature__regionSample2(true), 
		node_regionSample2(912), 
		acceleratorSignature__diffCalc2(true), 
		node_diffCalc2(913), 
		acceleratorSignature__targetReader3(true), 
		node_targetReader3(911), 
		acceleratorSignature__resultReader3(true), 
		node_resultReader3(911), 
		acceleratorSignature__regionSample3(true), 
		node_regionSample3(912), 
		acceleratorSignature__diffCalc3(true), 
		node_diffCalc3(913), 
		acceleratorSignature__pickCandidate(true), 
		node_pickCandidate(914), 
		threadID(0)
	{
		Reset();
	}
};
inline void (*GAMHandler_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*) ;
inline void Cleanup_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance);
inline void StartEverythingHandler_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance);
inline void StopEverythingHandler_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance);
inline void ErrorHandler_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance);
inline void Wait_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance);
inline unsigned int DelayEstimate_sig__TexSynth2LCacc(int vectorSize)
{
	return 1;
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__atlasReader(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__targetReader0(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__resultReader0(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__regionSample0(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__diffCalc0(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__targetReader1(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__resultReader1(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__regionSample1(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__diffCalc1(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__targetReader2(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__resultReader2(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__regionSample2(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__diffCalc2(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__targetReader3(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__resultReader3(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__regionSample3(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__diffCalc3(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
inline void (*ProgressHandler_sig__TexSynth2LCacc__pickCandidate(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*);
//This is a procedurally generated interrupt-based program from a RAGraph data structure for function TexSynth2LCacc
inline void Wait_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance)
{
	while(1)
	{
		int thread = instance->threadID;
		InterruptArgs* args = 0;
		void (*progress)(InstanceData_sig__TexSynth2LCacc*);
		while((args = LWI_CheckInterrupt(thread)) == 0);
		switch(args->lcaccMode)
		{
			case(0):
				progress = GAMHandler_sig__TexSynth2LCacc(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__atlasReader):
				progress = ProgressHandler_sig__TexSynth2LCacc__atlasReader(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__targetReader0):
				progress = ProgressHandler_sig__TexSynth2LCacc__targetReader0(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__resultReader0):
				progress = ProgressHandler_sig__TexSynth2LCacc__resultReader0(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__regionSample0):
				progress = ProgressHandler_sig__TexSynth2LCacc__regionSample0(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc0):
				progress = ProgressHandler_sig__TexSynth2LCacc__diffCalc0(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__targetReader1):
				progress = ProgressHandler_sig__TexSynth2LCacc__targetReader1(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__resultReader1):
				progress = ProgressHandler_sig__TexSynth2LCacc__resultReader1(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__regionSample1):
				progress = ProgressHandler_sig__TexSynth2LCacc__regionSample1(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc1):
				progress = ProgressHandler_sig__TexSynth2LCacc__diffCalc1(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__targetReader2):
				progress = ProgressHandler_sig__TexSynth2LCacc__targetReader2(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__resultReader2):
				progress = ProgressHandler_sig__TexSynth2LCacc__resultReader2(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__regionSample2):
				progress = ProgressHandler_sig__TexSynth2LCacc__regionSample2(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc2):
				progress = ProgressHandler_sig__TexSynth2LCacc__diffCalc2(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__targetReader3):
				progress = ProgressHandler_sig__TexSynth2LCacc__targetReader3(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__resultReader3):
				progress = ProgressHandler_sig__TexSynth2LCacc__resultReader3(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__regionSample3):
				progress = ProgressHandler_sig__TexSynth2LCacc__regionSample3(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc3):
				progress = ProgressHandler_sig__TexSynth2LCacc__diffCalc3(instance, args);
				break;
			case(LCACC_CLASS_SIG__TexSynth2LCacc__pickCandidate):
				progress = ProgressHandler_sig__TexSynth2LCacc__pickCandidate(instance, args);
				break;
			default:
				ErrorHandler_sig__TexSynth2LCacc(instance);
				simics_assert(0);
		}
		LWI_ClearInterrupt(instance->threadID);
		if(progress)
		{
			progress(instance);
			return;
		}
	}
}
inline void (*GAMHandler_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*) 
{
	int i;
	int lcaccMode;
	int count;
	int lcaccID;
	switch(args->status)
	{
		case(LCACC_GAM_WAIT):
			lcaccMode = args->v[0];
			count = args->v[1];
			switch(lcaccMode)
			{
				case(911)://Mode: TexSynth2
					for(i = 0; i < 9; i++)
					{
						instance->reservedAccelerators++;
						LCAcc_Reserve(args->threadID, args->v[2 + 2 * i], 1);
					}
					break;
				case(912)://Mode: TexSynth3
					for(i = 0; i < 4; i++)
					{
						instance->reservedAccelerators++;
						LCAcc_Reserve(args->threadID, args->v[2 + 2 * i], 1);
					}
					break;
				case(913)://Mode: TexSynth4
					for(i = 0; i < 4; i++)
					{
						instance->reservedAccelerators++;
						LCAcc_Reserve(args->threadID, args->v[2 + 2 * i], 1);
					}
					break;
				case(914)://Mode: TexSynth5
					for(i = 0; i < 1; i++)
					{
						instance->reservedAccelerators++;
						LCAcc_Reserve(args->threadID, args->v[2 + 2 * i], 1);
					}
					break;
			}
			if(instance->reservedAccelerators == 18)
			{
				instance->reservedAccelerators = 0;
				LCAcc_Reserve(args->threadID, 0, DelayEstimate_sig__TexSynth2LCacc(instance->acceleratorVectorLength));
			}
			return 0;
			break;
		case(LCACC_GAM_GRANT):
			lcaccMode = args->v[0];
			lcaccID = args->v[1];
			switch(lcaccMode)
			{
				case(911):
					if(instance->allocatedAcceleratorCount__TexSynth2 == 9)
					{
						return ErrorHandler_sig__TexSynth2LCacc;
					}
					else
					{
						instance->pendingAccelerators++;
						instance->allocatedAcceleratorIDSet__TexSynth2[instance->allocatedAcceleratorCount__TexSynth2] = lcaccID;
						instance->allocatedAcceleratorCount__TexSynth2++;
					}
					break;
				case(912):
					if(instance->allocatedAcceleratorCount__TexSynth3 == 4)
					{
						return ErrorHandler_sig__TexSynth2LCacc;
					}
					else
					{
						instance->pendingAccelerators++;
						instance->allocatedAcceleratorIDSet__TexSynth3[instance->allocatedAcceleratorCount__TexSynth3] = lcaccID;
						instance->allocatedAcceleratorCount__TexSynth3++;
					}
					break;
				case(913):
					if(instance->allocatedAcceleratorCount__TexSynth4 == 4)
					{
						return ErrorHandler_sig__TexSynth2LCacc;
					}
					else
					{
						instance->pendingAccelerators++;
						instance->allocatedAcceleratorIDSet__TexSynth4[instance->allocatedAcceleratorCount__TexSynth4] = lcaccID;
						instance->allocatedAcceleratorCount__TexSynth4++;
					}
					break;
				case(914):
					if(instance->allocatedAcceleratorCount__TexSynth5 == 1)
					{
						return ErrorHandler_sig__TexSynth2LCacc;
					}
					else
					{
						instance->pendingAccelerators++;
						instance->allocatedAcceleratorIDSet__TexSynth5[instance->allocatedAcceleratorCount__TexSynth5] = lcaccID;
						instance->allocatedAcceleratorCount__TexSynth5++;
					}
					break;
				default:
					return ErrorHandler_sig__TexSynth2LCacc;
			}
			if(instance->allocatedAcceleratorCount__TexSynth2 == 9 && instance->allocatedAcceleratorCount__TexSynth3 == 4 && instance->allocatedAcceleratorCount__TexSynth4 == 4 && instance->allocatedAcceleratorCount__TexSynth5 == 1)
			{
				return StartEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
			break;
		case(LCACC_GAM_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
			break;
		case(LCACC_GAM_REVOKE)://assumed thus far never to occur
			return ErrorHandler_sig__TexSynth2LCacc;
			break;
		default:
			return ErrorHandler_sig__TexSynth2LCacc;
	}
}
inline void Cleanup_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance)
{
	int i;
	LWI_UnregisterInterruptHandler(instance->GAM_INTERACTION.threadID, 0);
	if(instance->allocatedAcceleratorIDSet__TexSynth2[0] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__atlasReader.threadID, instance->HandlerArgs__atlasReader.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__atlasReader.threadID, instance->HandlerArgs__atlasReader.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth2[1] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__targetReader0.threadID, instance->HandlerArgs__targetReader0.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__targetReader0.threadID, instance->HandlerArgs__targetReader0.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth2[2] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__resultReader0.threadID, instance->HandlerArgs__resultReader0.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__resultReader0.threadID, instance->HandlerArgs__resultReader0.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth2[3] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__targetReader1.threadID, instance->HandlerArgs__targetReader1.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__targetReader1.threadID, instance->HandlerArgs__targetReader1.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth2[4] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__resultReader1.threadID, instance->HandlerArgs__resultReader1.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__resultReader1.threadID, instance->HandlerArgs__resultReader1.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth2[5] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__targetReader2.threadID, instance->HandlerArgs__targetReader2.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__targetReader2.threadID, instance->HandlerArgs__targetReader2.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth2[6] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__resultReader2.threadID, instance->HandlerArgs__resultReader2.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__resultReader2.threadID, instance->HandlerArgs__resultReader2.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth2[7] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__targetReader3.threadID, instance->HandlerArgs__targetReader3.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__targetReader3.threadID, instance->HandlerArgs__targetReader3.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth2[8] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__resultReader3.threadID, instance->HandlerArgs__resultReader3.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__resultReader3.threadID, instance->HandlerArgs__resultReader3.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth3[0] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__regionSample0.threadID, instance->HandlerArgs__regionSample0.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__regionSample0.threadID, instance->HandlerArgs__regionSample0.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth3[1] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__regionSample1.threadID, instance->HandlerArgs__regionSample1.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__regionSample1.threadID, instance->HandlerArgs__regionSample1.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth3[2] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__regionSample2.threadID, instance->HandlerArgs__regionSample2.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__regionSample2.threadID, instance->HandlerArgs__regionSample2.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth3[3] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__regionSample3.threadID, instance->HandlerArgs__regionSample3.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__regionSample3.threadID, instance->HandlerArgs__regionSample3.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth4[0] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__diffCalc0.threadID, instance->HandlerArgs__diffCalc0.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__diffCalc0.threadID, instance->HandlerArgs__diffCalc0.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth4[1] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__diffCalc1.threadID, instance->HandlerArgs__diffCalc1.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__diffCalc1.threadID, instance->HandlerArgs__diffCalc1.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth4[2] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__diffCalc2.threadID, instance->HandlerArgs__diffCalc2.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__diffCalc2.threadID, instance->HandlerArgs__diffCalc2.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth4[3] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__diffCalc3.threadID, instance->HandlerArgs__diffCalc3.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__diffCalc3.threadID, instance->HandlerArgs__diffCalc3.lcaccID);
	}
	if(instance->allocatedAcceleratorIDSet__TexSynth5[0] != 0)
	{
		LCAcc_Free(instance->HandlerArgs__pickCandidate.threadID, instance->HandlerArgs__pickCandidate.lcaccID);
		LWI_UnregisterInterruptHandler(instance->HandlerArgs__pickCandidate.threadID, instance->HandlerArgs__pickCandidate.lcaccID);
	}
}
inline void StopEverythingHandler_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance)
{
	Cleanup_sig__TexSynth2LCacc(instance);
}
inline void ErrorHandler_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance)
{
	Cleanup_sig__TexSynth2LCacc(instance);
	simics_assert(0);
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__atlasReader(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__targetReader0(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__resultReader0(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__regionSample0(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__diffCalc0(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__targetReader1(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__resultReader1(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__regionSample1(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__diffCalc1(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__targetReader2(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__resultReader2(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__regionSample2(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__diffCalc2(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__targetReader3(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__resultReader3(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__regionSample3(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__diffCalc3(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void (*ProgressHandler_sig__TexSynth2LCacc__pickCandidate(InstanceData_sig__TexSynth2LCacc* instance, InterruptArgs* args))(InstanceData_sig__TexSynth2LCacc*)
{
	switch(args->status)
	{
		case(LCACC_STATUS_TLB_MISS):
			LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
			return 0;
		case(LCACC_STATUS_COMPLETED):
			instance->pendingAccelerators--;
			if(instance->pendingAccelerators == 0)
			{
				return StopEverythingHandler_sig__TexSynth2LCacc;
			}
			else
			{
				return 0;
			}
		case(LCACC_STATUS_ERROR):
			return ErrorHandler_sig__TexSynth2LCacc;
		default:
			simics_assert(0);
			return 0;
	}
}
inline void StartEverythingHandler_sig__TexSynth2LCacc(InstanceData_sig__TexSynth2LCacc* instance)
{
	int index;
	instance->HandlerArgs__atlasReader.threadID = instance->threadID;
	instance->HandlerArgs__atlasReader.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[0];
	instance->node_atlasReader.SetLCAccID(instance->HandlerArgs__atlasReader.lcaccID);
	instance->HandlerArgs__atlasReader.status = 0;
	instance->HandlerArgs__atlasReader.taskIndex = 0;
	instance->HandlerArgs__atlasReader.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__atlasReader;
	instance->HandlerArgs__targetReader0.threadID = instance->threadID;
	instance->HandlerArgs__targetReader0.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[1];
	instance->node_targetReader0.SetLCAccID(instance->HandlerArgs__targetReader0.lcaccID);
	instance->HandlerArgs__targetReader0.status = 0;
	instance->HandlerArgs__targetReader0.taskIndex = 0;
	instance->HandlerArgs__targetReader0.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__targetReader0;
	instance->HandlerArgs__resultReader0.threadID = instance->threadID;
	instance->HandlerArgs__resultReader0.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[2];
	instance->node_resultReader0.SetLCAccID(instance->HandlerArgs__resultReader0.lcaccID);
	instance->HandlerArgs__resultReader0.status = 0;
	instance->HandlerArgs__resultReader0.taskIndex = 0;
	instance->HandlerArgs__resultReader0.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__resultReader0;
	instance->HandlerArgs__targetReader1.threadID = instance->threadID;
	instance->HandlerArgs__targetReader1.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[3];
	instance->node_targetReader1.SetLCAccID(instance->HandlerArgs__targetReader1.lcaccID);
	instance->HandlerArgs__targetReader1.status = 0;
	instance->HandlerArgs__targetReader1.taskIndex = 0;
	instance->HandlerArgs__targetReader1.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__targetReader1;
	instance->HandlerArgs__resultReader1.threadID = instance->threadID;
	instance->HandlerArgs__resultReader1.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[4];
	instance->node_resultReader1.SetLCAccID(instance->HandlerArgs__resultReader1.lcaccID);
	instance->HandlerArgs__resultReader1.status = 0;
	instance->HandlerArgs__resultReader1.taskIndex = 0;
	instance->HandlerArgs__resultReader1.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__resultReader1;
	instance->HandlerArgs__targetReader2.threadID = instance->threadID;
	instance->HandlerArgs__targetReader2.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[5];
	instance->node_targetReader2.SetLCAccID(instance->HandlerArgs__targetReader2.lcaccID);
	instance->HandlerArgs__targetReader2.status = 0;
	instance->HandlerArgs__targetReader2.taskIndex = 0;
	instance->HandlerArgs__targetReader2.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__targetReader2;
	instance->HandlerArgs__resultReader2.threadID = instance->threadID;
	instance->HandlerArgs__resultReader2.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[6];
	instance->node_resultReader2.SetLCAccID(instance->HandlerArgs__resultReader2.lcaccID);
	instance->HandlerArgs__resultReader2.status = 0;
	instance->HandlerArgs__resultReader2.taskIndex = 0;
	instance->HandlerArgs__resultReader2.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__resultReader2;
	instance->HandlerArgs__targetReader3.threadID = instance->threadID;
	instance->HandlerArgs__targetReader3.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[7];
	instance->node_targetReader3.SetLCAccID(instance->HandlerArgs__targetReader3.lcaccID);
	instance->HandlerArgs__targetReader3.status = 0;
	instance->HandlerArgs__targetReader3.taskIndex = 0;
	instance->HandlerArgs__targetReader3.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__targetReader3;
	instance->HandlerArgs__resultReader3.threadID = instance->threadID;
	instance->HandlerArgs__resultReader3.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth2[8];
	instance->node_resultReader3.SetLCAccID(instance->HandlerArgs__resultReader3.lcaccID);
	instance->HandlerArgs__resultReader3.status = 0;
	instance->HandlerArgs__resultReader3.taskIndex = 0;
	instance->HandlerArgs__resultReader3.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__resultReader3;
	instance->HandlerArgs__regionSample0.threadID = instance->threadID;
	instance->HandlerArgs__regionSample0.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth3[0];
	instance->node_regionSample0.SetLCAccID(instance->HandlerArgs__regionSample0.lcaccID);
	instance->HandlerArgs__regionSample0.status = 0;
	instance->HandlerArgs__regionSample0.taskIndex = 0;
	instance->HandlerArgs__regionSample0.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__regionSample0;
	instance->HandlerArgs__regionSample1.threadID = instance->threadID;
	instance->HandlerArgs__regionSample1.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth3[1];
	instance->node_regionSample1.SetLCAccID(instance->HandlerArgs__regionSample1.lcaccID);
	instance->HandlerArgs__regionSample1.status = 0;
	instance->HandlerArgs__regionSample1.taskIndex = 0;
	instance->HandlerArgs__regionSample1.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__regionSample1;
	instance->HandlerArgs__regionSample2.threadID = instance->threadID;
	instance->HandlerArgs__regionSample2.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth3[2];
	instance->node_regionSample2.SetLCAccID(instance->HandlerArgs__regionSample2.lcaccID);
	instance->HandlerArgs__regionSample2.status = 0;
	instance->HandlerArgs__regionSample2.taskIndex = 0;
	instance->HandlerArgs__regionSample2.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__regionSample2;
	instance->HandlerArgs__regionSample3.threadID = instance->threadID;
	instance->HandlerArgs__regionSample3.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth3[3];
	instance->node_regionSample3.SetLCAccID(instance->HandlerArgs__regionSample3.lcaccID);
	instance->HandlerArgs__regionSample3.status = 0;
	instance->HandlerArgs__regionSample3.taskIndex = 0;
	instance->HandlerArgs__regionSample3.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__regionSample3;
	instance->HandlerArgs__diffCalc0.threadID = instance->threadID;
	instance->HandlerArgs__diffCalc0.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth4[0];
	instance->node_diffCalc0.SetLCAccID(instance->HandlerArgs__diffCalc0.lcaccID);
	instance->HandlerArgs__diffCalc0.status = 0;
	instance->HandlerArgs__diffCalc0.taskIndex = 0;
	instance->HandlerArgs__diffCalc0.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc0;
	instance->HandlerArgs__diffCalc1.threadID = instance->threadID;
	instance->HandlerArgs__diffCalc1.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth4[1];
	instance->node_diffCalc1.SetLCAccID(instance->HandlerArgs__diffCalc1.lcaccID);
	instance->HandlerArgs__diffCalc1.status = 0;
	instance->HandlerArgs__diffCalc1.taskIndex = 0;
	instance->HandlerArgs__diffCalc1.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc1;
	instance->HandlerArgs__diffCalc2.threadID = instance->threadID;
	instance->HandlerArgs__diffCalc2.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth4[2];
	instance->node_diffCalc2.SetLCAccID(instance->HandlerArgs__diffCalc2.lcaccID);
	instance->HandlerArgs__diffCalc2.status = 0;
	instance->HandlerArgs__diffCalc2.taskIndex = 0;
	instance->HandlerArgs__diffCalc2.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc2;
	instance->HandlerArgs__diffCalc3.threadID = instance->threadID;
	instance->HandlerArgs__diffCalc3.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth4[3];
	instance->node_diffCalc3.SetLCAccID(instance->HandlerArgs__diffCalc3.lcaccID);
	instance->HandlerArgs__diffCalc3.status = 0;
	instance->HandlerArgs__diffCalc3.taskIndex = 0;
	instance->HandlerArgs__diffCalc3.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__diffCalc3;
	instance->HandlerArgs__pickCandidate.threadID = instance->threadID;
	instance->HandlerArgs__pickCandidate.lcaccID = instance->allocatedAcceleratorIDSet__TexSynth5[0];
	instance->node_pickCandidate.SetLCAccID(instance->HandlerArgs__pickCandidate.lcaccID);
	instance->HandlerArgs__pickCandidate.status = 0;
	instance->HandlerArgs__pickCandidate.taskIndex = 0;
	instance->HandlerArgs__pickCandidate.lcaccMode = LCACC_CLASS_SIG__TexSynth2LCacc__pickCandidate;
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__atlasReader));
	simics_assert(instance->acceleratorSignature__atlasReader.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__atlasReader.threadID, instance->HandlerArgs__atlasReader.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__atlasReader.GetBuffer(), instance->acceleratorSignature__atlasReader.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__targetReader0));
	simics_assert(instance->acceleratorSignature__targetReader0.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__targetReader0.threadID, instance->HandlerArgs__targetReader0.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__targetReader0.GetBuffer(), instance->acceleratorSignature__targetReader0.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__resultReader0));
	simics_assert(instance->acceleratorSignature__resultReader0.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__resultReader0.threadID, instance->HandlerArgs__resultReader0.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__resultReader0.GetBuffer(), instance->acceleratorSignature__resultReader0.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__regionSample0));
	simics_assert(instance->acceleratorSignature__regionSample0.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__regionSample0.threadID, instance->HandlerArgs__regionSample0.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__regionSample0.GetBuffer(), instance->acceleratorSignature__regionSample0.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__diffCalc0));
	simics_assert(instance->acceleratorSignature__diffCalc0.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__diffCalc0.threadID, instance->HandlerArgs__diffCalc0.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__diffCalc0.GetBuffer(), instance->acceleratorSignature__diffCalc0.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__targetReader1));
	simics_assert(instance->acceleratorSignature__targetReader1.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__targetReader1.threadID, instance->HandlerArgs__targetReader1.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__targetReader1.GetBuffer(), instance->acceleratorSignature__targetReader1.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__resultReader1));
	simics_assert(instance->acceleratorSignature__resultReader1.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__resultReader1.threadID, instance->HandlerArgs__resultReader1.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__resultReader1.GetBuffer(), instance->acceleratorSignature__resultReader1.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__regionSample1));
	simics_assert(instance->acceleratorSignature__regionSample1.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__regionSample1.threadID, instance->HandlerArgs__regionSample1.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__regionSample1.GetBuffer(), instance->acceleratorSignature__regionSample1.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__diffCalc1));
	simics_assert(instance->acceleratorSignature__diffCalc1.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__diffCalc1.threadID, instance->HandlerArgs__diffCalc1.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__diffCalc1.GetBuffer(), instance->acceleratorSignature__diffCalc1.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__targetReader2));
	simics_assert(instance->acceleratorSignature__targetReader2.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__targetReader2.threadID, instance->HandlerArgs__targetReader2.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__targetReader2.GetBuffer(), instance->acceleratorSignature__targetReader2.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__resultReader2));
	simics_assert(instance->acceleratorSignature__resultReader2.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__resultReader2.threadID, instance->HandlerArgs__resultReader2.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__resultReader2.GetBuffer(), instance->acceleratorSignature__resultReader2.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__regionSample2));
	simics_assert(instance->acceleratorSignature__regionSample2.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__regionSample2.threadID, instance->HandlerArgs__regionSample2.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__regionSample2.GetBuffer(), instance->acceleratorSignature__regionSample2.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__diffCalc2));
	simics_assert(instance->acceleratorSignature__diffCalc2.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__diffCalc2.threadID, instance->HandlerArgs__diffCalc2.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__diffCalc2.GetBuffer(), instance->acceleratorSignature__diffCalc2.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__targetReader3));
	simics_assert(instance->acceleratorSignature__targetReader3.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__targetReader3.threadID, instance->HandlerArgs__targetReader3.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__targetReader3.GetBuffer(), instance->acceleratorSignature__targetReader3.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__resultReader3));
	simics_assert(instance->acceleratorSignature__resultReader3.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__resultReader3.threadID, instance->HandlerArgs__resultReader3.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__resultReader3.GetBuffer(), instance->acceleratorSignature__resultReader3.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__regionSample3));
	simics_assert(instance->acceleratorSignature__regionSample3.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__regionSample3.threadID, instance->HandlerArgs__regionSample3.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__regionSample3.GetBuffer(), instance->acceleratorSignature__regionSample3.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__diffCalc3));
	simics_assert(instance->acceleratorSignature__diffCalc3.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__diffCalc3.threadID, instance->HandlerArgs__diffCalc3.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__diffCalc3.GetBuffer(), instance->acceleratorSignature__diffCalc3.GetBufferSize(), 0, 0);
	LWI_RegisterInterruptHandler(&(instance->HandlerArgs__pickCandidate));
	simics_assert(instance->acceleratorSignature__pickCandidate.IsFinalized());
	LCAcc_Command(instance->HandlerArgs__pickCandidate.threadID, instance->HandlerArgs__pickCandidate.lcaccID, LCACC_CMD_BEGIN_TASK_SIGNATURE, instance->acceleratorSignature__pickCandidate.GetBuffer(), instance->acceleratorSignature__pickCandidate.GetBufferSize(), 0, 0);
	Wait_sig__TexSynth2LCacc(instance);// wait for everything to finish
}
inline void CreateBuffer_TexSynth2LCacc_sig(int thread, InstanceData_sig__TexSynth2LCacc* instance, float (*resultImage), int inputHeight, int inputWidth, int outputHeight, int outputWidth, int imageCount, intptr_t imageArrayStart, intptr_t targetArrayStart, intptr_t atlasArray, uint32_t lineNumber, int chunk)
{
	int index, i;
	instance->binBufSize = 0;
	instance->threadID = thread;
	instance->GAM_INTERACTION.threadID = thread;
	instance->GAM_INTERACTION.lcaccID = 0;
	instance->GAM_INTERACTION.lcaccMode = 0;
	instance->node_atlasReader.SetSPMConfig((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_targetReader0.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_resultReader0.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_regionSample0.SetSPMConfig((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_diffCalc0.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_targetReader1.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_resultReader1.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_regionSample1.SetSPMConfig((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_diffCalc1.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_targetReader2.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_resultReader2.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_regionSample2.SetSPMConfig((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_diffCalc2.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_targetReader3.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_resultReader3.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_regionSample3.SetSPMConfig((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_diffCalc3.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->node_pickCandidate.SetSPMConfig((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0);
	instance->LCAcc_FuncArgs__resultImage = resultImage;
	instance->LCAcc_FuncVars__inputHeight = inputHeight;
	instance->LCAcc_FuncVars__inputWidth = inputWidth;
	instance->LCAcc_FuncVars__outputHeight = outputHeight;
	instance->LCAcc_FuncVars__outputWidth = outputWidth;
	instance->LCAcc_FuncVars__imageCount = imageCount;
	instance->LCAcc_FuncVars__imageArrayStart = imageArrayStart;
	instance->LCAcc_FuncVars__targetArrayStart = targetArrayStart;
	instance->LCAcc_FuncVars__atlasArray = atlasArray;
	instance->LCAcc_FuncVars__lineNumber = lineNumber;
	instance->LCAcc_FuncVars__chunk = chunk;
	instance->allocatedAcceleratorCount__TexSynth2 = 0;
	instance->allocatedAcceleratorCount__TexSynth3 = 0;
	instance->allocatedAcceleratorCount__TexSynth4 = 0;
	instance->allocatedAcceleratorCount__TexSynth5 = 0;
	LCAccNode& VNR_vardecl_0(instance->node_atlasReader);
	LCAccNode& VNR_vardecl_1(instance->node_regionSample0);
	LCAccNode& VNR_vardecl_2(instance->node_regionSample1);
	LCAccNode& VNR_vardecl_3(instance->node_regionSample2);
	LCAccNode& VNR_vardecl_4(instance->node_regionSample3);
	std::vector<uint32_t> VNR_vardecl_5;
	VNR_vardecl_5.push_back(((1) - (0)) / (1));
	VNR_vardecl_5.push_back(((1) - (0)) / (1));
	VNR_vardecl_5.push_back(((chunk) - (0)) / (1));
	std::vector<int32_t> VNR_vardecl_6;
	VNR_vardecl_6.push_back((1) * (sizeof(float) * (1) * (chunk)));
	VNR_vardecl_6.push_back((1) * (sizeof(float) * (chunk)));
	VNR_vardecl_6.push_back((1) * (sizeof(float)));
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_7(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_8;
	VNR_vardecl_8.push_back(VNR_vardecl_7);
	std::vector<uint64_t> VNR_vardecl_9;
	VNR_vardecl_9.push_back(ConvertToType<uint32_t, uint64_t>((0))/*Register output0*/);
	VNR_vardecl_9.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register output1*/);
	VNR_vardecl_9.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register output2*/);
	VNR_vardecl_9.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register output3*/);
	VNR_vardecl_9.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX0*/);
	VNR_vardecl_9.push_back(ConvertToType<int32_t, uint64_t>(-2)/*Register offsetY0*/);
	VNR_vardecl_9.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX1*/);
	VNR_vardecl_9.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY1*/);
	VNR_vardecl_9.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetX2*/);
	VNR_vardecl_9.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY2*/);
	VNR_vardecl_9.push_back(ConvertToType<int32_t, uint64_t>(1)/*Register offsetX3*/);
	VNR_vardecl_9.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY3*/);
	VNR_vardecl_9.push_back(ConvertToType<uint32_t, uint64_t>(4)/*Register elementSize*/);
	VNR_vardecl_9.push_back(ConvertToType<uint32_t, uint64_t>(2)/*Register elementCount*/);
	VNR_vardecl_9.push_back(ConvertToType<intptr_t, uint64_t>(atlasArray)/*Register inputAddr*/);
	VNR_vardecl_9.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register width*/);
	VNR_vardecl_9.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register height*/);
	VNR_vardecl_9.push_back(ConvertToType<int, uint64_t>(lineNumber)/*Register line*/);
	std::vector<uint32_t> VNR_vardecl_10;
	VNR_vardecl_10.push_back(((1) - (0)) / (1));
	VNR_vardecl_10.push_back(((1) - (0)) / (1));
	VNR_vardecl_10.push_back(((chunk) - (0)) / (1));
	VNR_vardecl_10.push_back((2));
	std::vector<int32_t> VNR_vardecl_11;
	VNR_vardecl_11.push_back((1) * ((sizeof(int32_t[2])) * (1) * (chunk)));
	VNR_vardecl_11.push_back((1) * ((sizeof(int32_t[2])) * (chunk)));
	VNR_vardecl_11.push_back((1) * ((sizeof(int32_t[2]))));
	VNR_vardecl_11.push_back((sizeof(int32_t)));
	LCAccNode& VNR_vardecl_12(instance->node_targetReader0);
	LCAccNode& VNR_vardecl_13(instance->node_diffCalc0);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_14((0) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_15;
	VNR_vardecl_15.push_back(VNR_vardecl_14);
	std::vector<uint64_t> VNR_vardecl_16;
	VNR_vardecl_16.push_back(ConvertToType<uint32_t, uint64_t>((0))/*Register output0*/);
	VNR_vardecl_16.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk))))/*Register output1*/);
	VNR_vardecl_16.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register output2*/);
	VNR_vardecl_16.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register output3*/);
	VNR_vardecl_16.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX0*/);
	VNR_vardecl_16.push_back(ConvertToType<int32_t, uint64_t>(-2)/*Register offsetY0*/);
	VNR_vardecl_16.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX1*/);
	VNR_vardecl_16.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY1*/);
	VNR_vardecl_16.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetX2*/);
	VNR_vardecl_16.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY2*/);
	VNR_vardecl_16.push_back(ConvertToType<int32_t, uint64_t>(1)/*Register offsetX3*/);
	VNR_vardecl_16.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY3*/);
	VNR_vardecl_16.push_back(ConvertToType<uint32_t, uint64_t>(4)/*Register elementSize*/);
	VNR_vardecl_16.push_back(ConvertToType<uint32_t, uint64_t>(1)/*Register elementCount*/);
	VNR_vardecl_16.push_back(ConvertToType<intptr_t, uint64_t>(targetArrayStart)/*Register inputAddr*/);
	VNR_vardecl_16.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register width*/);
	VNR_vardecl_16.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register height*/);
	VNR_vardecl_16.push_back(ConvertToType<int, uint64_t>(lineNumber)/*Register line*/);
	std::vector<int32_t> VNR_vardecl_17;
	VNR_vardecl_17.push_back((1) * ((sizeof(float)) * (1) * (chunk)));
	VNR_vardecl_17.push_back((1) * ((sizeof(float)) * (chunk)));
	VNR_vardecl_17.push_back((1) * ((sizeof(float))));
	LCAccNode& VNR_vardecl_18(instance->node_resultReader0);
	std::vector<uint64_t> VNR_vardecl_19;
	VNR_vardecl_19.push_back(ConvertToType<uint32_t, uint64_t>((0))/*Register output0*/);
	VNR_vardecl_19.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk))))/*Register output1*/);
	VNR_vardecl_19.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register output2*/);
	VNR_vardecl_19.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register output3*/);
	VNR_vardecl_19.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX0*/);
	VNR_vardecl_19.push_back(ConvertToType<int32_t, uint64_t>(-2)/*Register offsetY0*/);
	VNR_vardecl_19.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX1*/);
	VNR_vardecl_19.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY1*/);
	VNR_vardecl_19.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetX2*/);
	VNR_vardecl_19.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY2*/);
	VNR_vardecl_19.push_back(ConvertToType<int32_t, uint64_t>(1)/*Register offsetX3*/);
	VNR_vardecl_19.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY3*/);
	VNR_vardecl_19.push_back(ConvertToType<uint32_t, uint64_t>(4)/*Register elementSize*/);
	VNR_vardecl_19.push_back(ConvertToType<uint32_t, uint64_t>(1)/*Register elementCount*/);
	VNR_vardecl_19.push_back(ConvertToType<intptr_t, uint64_t>((uint64_t)resultImage)/*Register inputAddr*/);
	VNR_vardecl_19.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register width*/);
	VNR_vardecl_19.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register height*/);
	VNR_vardecl_19.push_back(ConvertToType<int, uint64_t>(lineNumber)/*Register line*/);
	LCAccNode& VNR_vardecl_20(instance->node_targetReader1);
	LCAccNode& VNR_vardecl_21(instance->node_diffCalc1);
	LCAccNode& VNR_vardecl_22(instance->node_resultReader1);
	LCAccNode& VNR_vardecl_23(instance->node_targetReader2);
	LCAccNode& VNR_vardecl_24(instance->node_diffCalc2);
	LCAccNode& VNR_vardecl_25(instance->node_resultReader2);
	LCAccNode& VNR_vardecl_26(instance->node_targetReader3);
	LCAccNode& VNR_vardecl_27(instance->node_diffCalc3);
	LCAccNode& VNR_vardecl_28(instance->node_resultReader3);
	LCAccNode& VNR_vardecl_29(instance->node_pickCandidate);
	std::vector<int32_t> VNR_vardecl_30;
	VNR_vardecl_30.push_back((1) * (sizeof(int32_t[2]) * (1) * (chunk)));
	VNR_vardecl_30.push_back((1) * (sizeof(int32_t[2]) * (chunk)));
	VNR_vardecl_30.push_back((1) * (sizeof(int32_t[2])));
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_31((0) + ((((0) * (sizeof(int32_t[2]) * (1) * (chunk))) + ((0) * (sizeof(int32_t[2]) * (chunk))) + ((0) * (sizeof(int32_t[2]))))), sizeof(int32_t), VNR_vardecl_5, VNR_vardecl_30);
	std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_32;
	VNR_vardecl_32.push_back(VNR_vardecl_31);
	std::vector<uint64_t> VNR_vardecl_33;
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(1)/*Register xBias*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(0)/*Register yBias*/);
	VNR_vardecl_33.push_back(ConvertToType<intptr_t, uint64_t>(imageArrayStart)/*Register sourceImageAddr*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(inputWidth)/*Register sourceImageWidth*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(inputHeight)/*Register sourceImageHeight*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register dstImageWidth*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register dstImageHeight*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias0*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(-2)/*Register candidateYBias0*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias1*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias1*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias2*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias2*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias3*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias3*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias4*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias4*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias5*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias5*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias6*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias6*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias7*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias7*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias8*/);
	VNR_vardecl_33.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias8*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register targetSPMLocation0*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation1*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation2*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation3*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation4*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation5*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation6*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation7*/);
	VNR_vardecl_33.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation8*/);
	std::vector<uint64_t> VNR_vardecl_34;
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(1)/*Register xBias*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(1)/*Register yBias*/);
	VNR_vardecl_34.push_back(ConvertToType<intptr_t, uint64_t>(imageArrayStart)/*Register sourceImageAddr*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(inputWidth)/*Register sourceImageWidth*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(inputHeight)/*Register sourceImageHeight*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register dstImageWidth*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register dstImageHeight*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias0*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(-2)/*Register candidateYBias0*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias1*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias1*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias2*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias2*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias3*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias3*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias4*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias4*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias5*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias5*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias6*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias6*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias7*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias7*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias8*/);
	VNR_vardecl_34.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias8*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register targetSPMLocation0*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation1*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation2*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation3*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation4*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation5*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation6*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation7*/);
	VNR_vardecl_34.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation8*/);
	std::vector<uint64_t> VNR_vardecl_35;
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(0)/*Register xBias*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(1)/*Register yBias*/);
	VNR_vardecl_35.push_back(ConvertToType<intptr_t, uint64_t>(imageArrayStart)/*Register sourceImageAddr*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(inputWidth)/*Register sourceImageWidth*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(inputHeight)/*Register sourceImageHeight*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register dstImageWidth*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register dstImageHeight*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias0*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(-2)/*Register candidateYBias0*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias1*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias1*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias2*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias2*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias3*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias3*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias4*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias4*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias5*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias5*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias6*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias6*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias7*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias7*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias8*/);
	VNR_vardecl_35.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias8*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register targetSPMLocation0*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation1*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation2*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation3*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation4*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation5*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation6*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation7*/);
	VNR_vardecl_35.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation8*/);
	std::vector<uint64_t> VNR_vardecl_36;
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(-1)/*Register xBias*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(1)/*Register yBias*/);
	VNR_vardecl_36.push_back(ConvertToType<intptr_t, uint64_t>(imageArrayStart)/*Register sourceImageAddr*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(inputWidth)/*Register sourceImageWidth*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(inputHeight)/*Register sourceImageHeight*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register dstImageWidth*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register dstImageHeight*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias0*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(-2)/*Register candidateYBias0*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias1*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias1*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias2*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias2*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias3*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias3*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias4*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias4*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias5*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias5*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias6*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias6*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias7*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias7*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias8*/);
	VNR_vardecl_36.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias8*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register targetSPMLocation0*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation1*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation2*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation3*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation4*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation5*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation6*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation7*/);
	VNR_vardecl_36.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation8*/);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_37(((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_38(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_39(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_40(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_41(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_42(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_43(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_44(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_45(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_46(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_47(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_48(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_49(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_50(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_51(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	MicroprogramWriter::ComputeArgIndex VNR_vardecl_52(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_5, VNR_vardecl_6);
	std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_53;
	VNR_vardecl_53.push_back(VNR_vardecl_14);
	VNR_vardecl_53.push_back(VNR_vardecl_37);
	VNR_vardecl_53.push_back(VNR_vardecl_38);
	VNR_vardecl_53.push_back(VNR_vardecl_39);
	VNR_vardecl_53.push_back(VNR_vardecl_40);
	VNR_vardecl_53.push_back(VNR_vardecl_41);
	VNR_vardecl_53.push_back(VNR_vardecl_42);
	VNR_vardecl_53.push_back(VNR_vardecl_43);
	VNR_vardecl_53.push_back(VNR_vardecl_44);
	VNR_vardecl_53.push_back(VNR_vardecl_45);
	VNR_vardecl_53.push_back(VNR_vardecl_46);
	VNR_vardecl_53.push_back(VNR_vardecl_47);
	VNR_vardecl_53.push_back(VNR_vardecl_48);
	VNR_vardecl_53.push_back(VNR_vardecl_49);
	VNR_vardecl_53.push_back(VNR_vardecl_50);
	VNR_vardecl_53.push_back(VNR_vardecl_51);
	VNR_vardecl_53.push_back(VNR_vardecl_52);
	std::vector<uint64_t> VNR_vardecl_54;
	void* VNR_vardecl_55(instance->LCAcc_FuncArgs__resultImage);
	std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_56;
	VNR_vardecl_56.push_back(VNR_vardecl_14);
	VNR_vardecl_56.push_back(VNR_vardecl_38);
	VNR_vardecl_56.push_back(VNR_vardecl_40);
	VNR_vardecl_56.push_back(VNR_vardecl_42);
	VNR_vardecl_56.push_back(VNR_vardecl_37);
	VNR_vardecl_56.push_back(VNR_vardecl_39);
	VNR_vardecl_56.push_back(VNR_vardecl_41);
	VNR_vardecl_56.push_back(VNR_vardecl_43);
	VNR_vardecl_56.push_back(VNR_vardecl_44);
	std::vector<uint32_t> VNR_vardecl_57;
	VNR_vardecl_57.push_back(((imageCount) - (0)) / (1));
	VNR_vardecl_57.push_back(((lineNumber + 1) - (lineNumber)) / (1));
	VNR_vardecl_57.push_back(((outputWidth) - (0)) / (chunk));
	std::vector<int32_t> VNR_vardecl_58;
	VNR_vardecl_58.push_back((1) * ((sizeof(float) * (outputHeight) * (outputWidth))));
	VNR_vardecl_58.push_back((1) * ((sizeof(float) * (outputWidth))));
	VNR_vardecl_58.push_back((chunk) * ((sizeof(float))));
	std::vector<int32_t> VNR_vardecl_59;
	VNR_vardecl_59.push_back((1) * ((sizeof(float) * (outputHeight) * (outputWidth))));
	VNR_vardecl_59.push_back((1) * ((sizeof(float) * (outputWidth))));
	VNR_vardecl_59.push_back((1) * ((sizeof(float))));
	//produce compute set
	//See VNR_vardecl_8 for index variable decl
	//See VNR_vardecl_9 for register set decl
	instance->acceleratorSignature__atlasReader.AddCompute(VNR_vardecl_0, VNR_vardecl_8, VNR_vardecl_9);
	//produce transfer set
	//transfer from atlasReader to regionSample0
	//Search VNR_vardecl_10 for source size.
	//Search VNR_vardecl_11 for source stride.
	//Search VNR_vardecl_10 for destination size.
	//Search VNR_vardecl_11 for destination stride.
	instance->acceleratorSignature__atlasReader.AddTransfer(VNR_vardecl_0, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, VNR_vardecl_1, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, sizeof(int32_t));
	//transfer from atlasReader to regionSample1
	//Search VNR_vardecl_10 for source size.
	//Search VNR_vardecl_11 for source stride.
	//Search VNR_vardecl_10 for destination size.
	//Search VNR_vardecl_11 for destination stride.
	instance->acceleratorSignature__atlasReader.AddTransfer(VNR_vardecl_0, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, VNR_vardecl_2, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, sizeof(int32_t));
	//transfer from atlasReader to regionSample2
	//Search VNR_vardecl_10 for source size.
	//Search VNR_vardecl_11 for source stride.
	//Search VNR_vardecl_10 for destination size.
	//Search VNR_vardecl_11 for destination stride.
	instance->acceleratorSignature__atlasReader.AddTransfer(VNR_vardecl_0, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, VNR_vardecl_3, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, sizeof(int32_t));
	//transfer from atlasReader to regionSample3
	//Search VNR_vardecl_10 for source size.
	//Search VNR_vardecl_11 for source stride.
	//Search VNR_vardecl_10 for destination size.
	//Search VNR_vardecl_11 for destination stride.
	instance->acceleratorSignature__atlasReader.AddTransfer(VNR_vardecl_0, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, VNR_vardecl_4, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, sizeof(int32_t));
	instance->acceleratorSignature__atlasReader.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_15 for index variable decl
	//See VNR_vardecl_16 for register set decl
	instance->acceleratorSignature__targetReader0.AddCompute(VNR_vardecl_12, VNR_vardecl_15, VNR_vardecl_16);
	//produce transfer set
	//transfer from targetReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader0.AddTransfer(VNR_vardecl_12, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader0.AddTransfer(VNR_vardecl_12, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader0.AddTransfer(VNR_vardecl_12, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader0.AddTransfer(VNR_vardecl_12, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__targetReader0.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_15 for index variable decl
	//See VNR_vardecl_19 for register set decl
	instance->acceleratorSignature__resultReader0.AddCompute(VNR_vardecl_18, VNR_vardecl_15, VNR_vardecl_19);
	//produce transfer set
	//transfer from resultReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader0.AddTransfer(VNR_vardecl_18, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader0.AddTransfer(VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader0.AddTransfer(VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader0.AddTransfer(VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__resultReader0.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_15 for index variable decl
	//See VNR_vardecl_16 for register set decl
	instance->acceleratorSignature__targetReader1.AddCompute(VNR_vardecl_20, VNR_vardecl_15, VNR_vardecl_16);
	//produce transfer set
	//transfer from targetReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader1.AddTransfer(VNR_vardecl_20, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader1.AddTransfer(VNR_vardecl_20, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader1.AddTransfer(VNR_vardecl_20, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader1.AddTransfer(VNR_vardecl_20, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__targetReader1.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_15 for index variable decl
	//See VNR_vardecl_19 for register set decl
	instance->acceleratorSignature__resultReader1.AddCompute(VNR_vardecl_22, VNR_vardecl_15, VNR_vardecl_19);
	//produce transfer set
	//transfer from resultReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader1.AddTransfer(VNR_vardecl_22, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader1.AddTransfer(VNR_vardecl_22, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader1.AddTransfer(VNR_vardecl_22, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader1.AddTransfer(VNR_vardecl_22, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__resultReader1.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_15 for index variable decl
	//See VNR_vardecl_16 for register set decl
	instance->acceleratorSignature__targetReader2.AddCompute(VNR_vardecl_23, VNR_vardecl_15, VNR_vardecl_16);
	//produce transfer set
	//transfer from targetReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader2.AddTransfer(VNR_vardecl_23, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader2.AddTransfer(VNR_vardecl_23, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader2.AddTransfer(VNR_vardecl_23, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader2.AddTransfer(VNR_vardecl_23, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__targetReader2.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_15 for index variable decl
	//See VNR_vardecl_19 for register set decl
	instance->acceleratorSignature__resultReader2.AddCompute(VNR_vardecl_25, VNR_vardecl_15, VNR_vardecl_19);
	//produce transfer set
	//transfer from resultReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader2.AddTransfer(VNR_vardecl_25, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader2.AddTransfer(VNR_vardecl_25, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader2.AddTransfer(VNR_vardecl_25, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader2.AddTransfer(VNR_vardecl_25, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__resultReader2.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_15 for index variable decl
	//See VNR_vardecl_16 for register set decl
	instance->acceleratorSignature__targetReader3.AddCompute(VNR_vardecl_26, VNR_vardecl_15, VNR_vardecl_16);
	//produce transfer set
	//transfer from targetReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader3.AddTransfer(VNR_vardecl_26, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader3.AddTransfer(VNR_vardecl_26, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader3.AddTransfer(VNR_vardecl_26, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__targetReader3.AddTransfer(VNR_vardecl_26, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__targetReader3.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_15 for index variable decl
	//See VNR_vardecl_19 for register set decl
	instance->acceleratorSignature__resultReader3.AddCompute(VNR_vardecl_28, VNR_vardecl_15, VNR_vardecl_19);
	//produce transfer set
	//transfer from resultReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader3.AddTransfer(VNR_vardecl_28, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader3.AddTransfer(VNR_vardecl_28, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader3.AddTransfer(VNR_vardecl_28, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__resultReader3.AddTransfer(VNR_vardecl_28, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__resultReader3.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_32 for index variable decl
	//See VNR_vardecl_33 for register set decl
	instance->acceleratorSignature__regionSample0.AddCompute(VNR_vardecl_1, VNR_vardecl_32, VNR_vardecl_33);
	//produce transfer set
	//transfer from atlasReader to regionSample0
	//Search VNR_vardecl_10 for source size.
	//Search VNR_vardecl_11 for source stride.
	//Search VNR_vardecl_10 for destination size.
	//Search VNR_vardecl_11 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_0, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, VNR_vardecl_1, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, sizeof(int32_t));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__regionSample0.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_32 for index variable decl
	//See VNR_vardecl_34 for register set decl
	instance->acceleratorSignature__regionSample1.AddCompute(VNR_vardecl_2, VNR_vardecl_32, VNR_vardecl_34);
	//produce transfer set
	//transfer from atlasReader to regionSample1
	//Search VNR_vardecl_10 for source size.
	//Search VNR_vardecl_11 for source stride.
	//Search VNR_vardecl_10 for destination size.
	//Search VNR_vardecl_11 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_0, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, VNR_vardecl_2, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, sizeof(int32_t));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__regionSample1.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_32 for index variable decl
	//See VNR_vardecl_35 for register set decl
	instance->acceleratorSignature__regionSample2.AddCompute(VNR_vardecl_3, VNR_vardecl_32, VNR_vardecl_35);
	//produce transfer set
	//transfer from atlasReader to regionSample2
	//Search VNR_vardecl_10 for source size.
	//Search VNR_vardecl_11 for source stride.
	//Search VNR_vardecl_10 for destination size.
	//Search VNR_vardecl_11 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_0, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, VNR_vardecl_3, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, sizeof(int32_t));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__regionSample2.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_32 for index variable decl
	//See VNR_vardecl_36 for register set decl
	instance->acceleratorSignature__regionSample3.AddCompute(VNR_vardecl_4, VNR_vardecl_32, VNR_vardecl_36);
	//produce transfer set
	//transfer from atlasReader to regionSample3
	//Search VNR_vardecl_10 for source size.
	//Search VNR_vardecl_11 for source stride.
	//Search VNR_vardecl_10 for destination size.
	//Search VNR_vardecl_11 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_0, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, VNR_vardecl_4, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_10, VNR_vardecl_11, sizeof(int32_t));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__regionSample3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__regionSample3.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_53 for index variable decl
	//See VNR_vardecl_54 for register set decl
	instance->acceleratorSignature__diffCalc0.AddCompute(VNR_vardecl_13, VNR_vardecl_53, VNR_vardecl_54);
	//produce transfer set
	//transfer from targetReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_12, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_12, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_12, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_12, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_18, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample0 to diffCalc0
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from diffCalc0 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc0.AddTransfer(VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__diffCalc0.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_53 for index variable decl
	//See VNR_vardecl_54 for register set decl
	instance->acceleratorSignature__diffCalc1.AddCompute(VNR_vardecl_21, VNR_vardecl_53, VNR_vardecl_54);
	//produce transfer set
	//transfer from targetReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_20, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_20, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_20, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_20, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_22, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_22, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_22, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_22, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to diffCalc1
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from diffCalc1 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc1.AddTransfer(VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__diffCalc1.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_53 for index variable decl
	//See VNR_vardecl_54 for register set decl
	instance->acceleratorSignature__diffCalc2.AddCompute(VNR_vardecl_24, VNR_vardecl_53, VNR_vardecl_54);
	//produce transfer set
	//transfer from targetReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_23, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_23, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_23, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_23, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_25, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_25, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_25, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_25, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to diffCalc2
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from diffCalc2 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc2.AddTransfer(VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__diffCalc2.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_53 for index variable decl
	//See VNR_vardecl_54 for register set decl
	instance->acceleratorSignature__diffCalc3.AddCompute(VNR_vardecl_27, VNR_vardecl_53, VNR_vardecl_54);
	//produce transfer set
	//transfer from targetReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_26, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_26, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_26, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from targetReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_26, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_28, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_28, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_28, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from resultReader3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_28, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to diffCalc3
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from diffCalc3 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__diffCalc3.AddTransfer(VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	instance->acceleratorSignature__diffCalc3.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	//produce compute set
	//See VNR_vardecl_56 for index variable decl
	//See VNR_vardecl_54 for register set decl
	instance->acceleratorSignature__pickCandidate.AddCompute(VNR_vardecl_29, VNR_vardecl_56, VNR_vardecl_54);
	//produce transfer set
	//transfer from regionSample0 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from diffCalc0 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample1 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_2, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from diffCalc1 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_21, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample2 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_3, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from diffCalc2 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_24, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from regionSample3 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from diffCalc3 to pickCandidate
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_5 for destination size.
	//Search VNR_vardecl_17 for destination stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_27, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, sizeof(float));
	//transfer from pickCandidate to resultImage
	//Search VNR_vardecl_5 for source size.
	//Search VNR_vardecl_17 for source stride.
	//Search VNR_vardecl_57 for destination block size.
	//Search VNR_vardecl_58 for destination block stride.
	//Search VNR_vardecl_5 for destination element size.
	//Search VNR_vardecl_59 for destination element stride.
	instance->acceleratorSignature__pickCandidate.AddTransfer(VNR_vardecl_29, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_5, VNR_vardecl_17, VNR_vardecl_55, VNR_vardecl_57, VNR_vardecl_58, VNR_vardecl_5, VNR_vardecl_59, sizeof(float));
	instance->acceleratorSignature__pickCandidate.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
}
inline void TexSynth2LCacc_sig_buf(int threadID, InstanceData_sig__TexSynth2LCacc* instance)
{
	instance->Reset();
	LWI_RegisterInterruptHandler(&(instance->GAM_INTERACTION));
	LCAcc_Request(threadID, 911, 1);//request something of type AcceleratorType
	LCAcc_Request(threadID, 912, 1);//request something of type AcceleratorType
	LCAcc_Request(threadID, 913, 1);//request something of type AcceleratorType
	LCAcc_Request(threadID, 914, 1);//request something of type AcceleratorType
	Wait_sig__TexSynth2LCacc(instance);// wait for everything to finish
}
inline void TexSynth2LCacc_sig(int threadID, float (*resultImage), int inputHeight, int inputWidth, int outputHeight, int outputWidth, int imageCount, intptr_t imageArrayStart, intptr_t targetArrayStart, intptr_t atlasArray, uint32_t lineNumber, int chunk)
{
	InstanceData_sig__TexSynth2LCacc instance;
	CreateBuffer_TexSynth2LCacc_sig(threadID, &instance, resultImage, inputHeight, inputWidth, outputHeight, outputWidth, imageCount, imageArrayStart, targetArrayStart, atlasArray, lineNumber, chunk);
	TexSynth2LCacc_sig_buf(threadID, &instance);
}
class BiN_TexSynth2LCacc_Arbitrator_sig
{
	std::vector<InstanceData_sig__TexSynth2LCacc*> instanceSet;
	std::vector<uint32_t> performancePoint;
	std::vector<uint32_t> cachePressureMod;
	std::vector<uint32_t> ops;
	std::vector<uint32_t> count;
	int threadID;
	int allocatedAcceleratorCount__TexSynth2;
	int allocatedAcceleratorIDSet__TexSynth2[9];
	int allocatedAcceleratorCount__TexSynth3;
	int allocatedAcceleratorIDSet__TexSynth3[4];
	int allocatedAcceleratorCount__TexSynth4;
	int allocatedAcceleratorIDSet__TexSynth4[4];
	int allocatedAcceleratorCount__TexSynth5;
	int allocatedAcceleratorIDSet__TexSynth5[1];
	InterruptArgs isr;
public:
	inline BiN_TexSynth2LCacc_Arbitrator_sig(int thread)
	{
		ops.push_back(911);
		count.push_back(9);
		ops.push_back(912);
		count.push_back(4);
		ops.push_back(913);
		count.push_back(4);
		ops.push_back(914);
		count.push_back(1);
		threadID = thread;
	}
	inline void AddConfig(InstanceData_sig__TexSynth2LCacc* inst, uint32_t performance, uint32_t cacheMod)
	{
		simics_assert(inst->acceleratorSignature__atlasReader.IsFinalized());
		simics_assert(inst->acceleratorSignature__targetReader0.IsFinalized());
		simics_assert(inst->acceleratorSignature__resultReader0.IsFinalized());
		simics_assert(inst->acceleratorSignature__regionSample0.IsFinalized());
		simics_assert(inst->acceleratorSignature__diffCalc0.IsFinalized());
		simics_assert(inst->acceleratorSignature__targetReader1.IsFinalized());
		simics_assert(inst->acceleratorSignature__resultReader1.IsFinalized());
		simics_assert(inst->acceleratorSignature__regionSample1.IsFinalized());
		simics_assert(inst->acceleratorSignature__diffCalc1.IsFinalized());
		simics_assert(inst->acceleratorSignature__targetReader2.IsFinalized());
		simics_assert(inst->acceleratorSignature__resultReader2.IsFinalized());
		simics_assert(inst->acceleratorSignature__regionSample2.IsFinalized());
		simics_assert(inst->acceleratorSignature__diffCalc2.IsFinalized());
		simics_assert(inst->acceleratorSignature__targetReader3.IsFinalized());
		simics_assert(inst->acceleratorSignature__resultReader3.IsFinalized());
		simics_assert(inst->acceleratorSignature__regionSample3.IsFinalized());
		simics_assert(inst->acceleratorSignature__diffCalc3.IsFinalized());
		simics_assert(inst->acceleratorSignature__pickCandidate.IsFinalized());
		instanceSet.push_back(inst);
		performancePoint.push_back(performance);
		cachePressureMod.push_back(cacheMod);
	}
	inline void Run()
	{
		isr.threadID = threadID;
		isr.lcaccID = 0;
		isr.lcaccMode = 0;
		for(size_t i = 0; i < instanceSet.size(); i++)
		{
			instanceSet[i]->Reset();
		}
		LWI_RegisterInterruptHandler(&isr);
		int allocatedAcceleratorCount__TexSynth2 = 0;
		int allocatedAcceleratorCount__TexSynth3 = 0;
		int allocatedAcceleratorCount__TexSynth4 = 0;
		int allocatedAcceleratorCount__TexSynth5 = 0;
		LCAcc_Request(threadID, 911, 1);//request something of type AcceleratorType
		LCAcc_Request(threadID, 912, 1);//request something of type AcceleratorType
		LCAcc_Request(threadID, 913, 1);//request something of type AcceleratorType
		LCAcc_Request(threadID, 914, 1);//request something of type AcceleratorType
		bool cont = true;
		uint32_t bufferSize = 0;
		bool bufKnown = false;
		uint32_t bufferID;
		int reserves = 0;
		while(cont)
		{
			InterruptArgs* args = 0;
			while((args = LWI_CheckInterrupt(threadID)) == 0);
			simics_assert(args == &isr);
			switch(args->status)
			{
				case(LCACC_GAM_WAIT):
					{
						int mode = args->v[0];
						switch(mode)
						{
							case(911)://Mode: TexSynth2
								for(int i = 0; i < 9; i++)
								{
									reserves++;
									LCAcc_Reserve(threadID, args->v[2 + 2 * i], 1);
								}
								break;
							case(912)://Mode: TexSynth3
								for(int i = 0; i < 4; i++)
								{
									reserves++;
									LCAcc_Reserve(threadID, args->v[2 + 2 * i], 1);
								}
								break;
							case(913)://Mode: TexSynth4
								for(int i = 0; i < 4; i++)
								{
									reserves++;
									LCAcc_Reserve(threadID, args->v[2 + 2 * i], 1);
								}
								break;
							case(914)://Mode: TexSynth5
								for(int i = 0; i < 1; i++)
								{
									reserves++;
									LCAcc_Reserve(threadID, args->v[2 + 2 * i], 1);
								}
								break;
						}
						if(reserves == 18)
						{
							for(size_t i = 0; i < instanceSet.size(); i++)
							{
								LCAcc_SendBiNCurve(threadID, instanceSet[i]->binBufSize, performancePoint[i], cachePressureMod[i]);
							}
							LCAcc_Reserve(threadID, 0, 1);
						}
					}
					break;
				case(LCACC_GAM_GRANT):
					{
						uint32_t lcaccMode = args->v[0];
						uint32_t lcaccID = args->v[1];
						uint32_t bufSize = args->v[3];
						if(!bufKnown)
						{
							bufferSize = bufSize;
							bufKnown = true;
						}
						else
						{
							simics_assert(bufferSize == bufSize);
						}
						switch(lcaccMode)
						{
							case(911):
								simics_assert(allocatedAcceleratorCount__TexSynth2 < 9);
								allocatedAcceleratorIDSet__TexSynth2[allocatedAcceleratorCount__TexSynth2] = lcaccID;
								allocatedAcceleratorCount__TexSynth2++;
								break;
							case(912):
								simics_assert(allocatedAcceleratorCount__TexSynth3 < 4);
								allocatedAcceleratorIDSet__TexSynth3[allocatedAcceleratorCount__TexSynth3] = lcaccID;
								allocatedAcceleratorCount__TexSynth3++;
								break;
							case(913):
								simics_assert(allocatedAcceleratorCount__TexSynth4 < 4);
								allocatedAcceleratorIDSet__TexSynth4[allocatedAcceleratorCount__TexSynth4] = lcaccID;
								allocatedAcceleratorCount__TexSynth4++;
								break;
							case(914):
								simics_assert(allocatedAcceleratorCount__TexSynth5 < 1);
								allocatedAcceleratorIDSet__TexSynth5[allocatedAcceleratorCount__TexSynth5] = lcaccID;
								allocatedAcceleratorCount__TexSynth5++;
								break;
							default:
								simics_assert(0);
						}
						if(allocatedAcceleratorCount__TexSynth2 == 9 && allocatedAcceleratorCount__TexSynth3 == 4 && allocatedAcceleratorCount__TexSynth4 == 4 && allocatedAcceleratorCount__TexSynth5 == 1)
						{
							cont = false;
						}
					}
					break;
				default:
					simics_assert(0);
			}
			LWI_ClearInterrupt(threadID);
		}
		simics_assert(bufKnown);
		InstanceData_sig__TexSynth2LCacc* inst = NULL;
		for(size_t i = 0; i < instanceSet.size(); i++)
		{
			if(instanceSet[i]->binBufSize == bufferSize)
			{
				inst = instanceSet[i];
				break;
			}
		}
		simics_assert(inst);
		for(int i = 0; i < 9; i++)
		{
			inst->allocatedAcceleratorIDSet__TexSynth2[i] = allocatedAcceleratorIDSet__TexSynth2[i];
		}
		inst->allocatedAcceleratorCount__TexSynth2 = allocatedAcceleratorCount__TexSynth2;
		for(int i = 0; i < 4; i++)
		{
			inst->allocatedAcceleratorIDSet__TexSynth3[i] = allocatedAcceleratorIDSet__TexSynth3[i];
		}
		inst->allocatedAcceleratorCount__TexSynth3 = allocatedAcceleratorCount__TexSynth3;
		for(int i = 0; i < 4; i++)
		{
			inst->allocatedAcceleratorIDSet__TexSynth4[i] = allocatedAcceleratorIDSet__TexSynth4[i];
		}
		inst->allocatedAcceleratorCount__TexSynth4 = allocatedAcceleratorCount__TexSynth4;
		for(int i = 0; i < 1; i++)
		{
			inst->allocatedAcceleratorIDSet__TexSynth5[i] = allocatedAcceleratorIDSet__TexSynth5[i];
		}
		inst->allocatedAcceleratorCount__TexSynth5 = allocatedAcceleratorCount__TexSynth5;
		inst->pendingAccelerators = 18;
		StartEverythingHandler_sig__TexSynth2LCacc(inst);
	}
};

#endif
#ifndef LCACC_BODY_TD__TexSynth2LCacc__X
#define LCACC_BODY_TD__TexSynth2LCacc__X

inline void Wait_td__TexSynth2LCacc(int thread)
{
	int stillWorking = (thread == 0) ? 0 : 1;
	while(stillWorking)
	{
		InterruptArgs* args = 0;
		while((args = LWI_CheckInterrupt(thread)) == 0);
		simics_assert(args->lcaccMode == 0);
		switch(args->status)
		{
			case(LCACC_STATUS_TLB_MISS):
				LCAcc_Command(args->threadID, args->lcaccID, LCACC_CMD_TLB_SERVICE, (void*)(args->v[0]), 0, 0, 0);
				break;
			case(LCACC_STATUS_COMPLETED):
				stillWorking = 0;
				break;
			default:
				simics_assert(0);
				stillWorking = 0;
		}
		LWI_ClearInterrupt(thread);
	}
}
inline void CreateBuffer_TexSynth2LCacc_td(uint8_t** buffer, uint32_t* bufferSize, uint8_t** constCluster, int thread, float (*resultImage), int inputHeight, int inputWidth, int outputHeight, int outputWidth, int imageCount, intptr_t imageArrayStart, intptr_t targetArrayStart, intptr_t atlasArray, uint32_t lineNumber, int chunk)
{
	MicroprogramWriter mw(false);
	void* LCAcc_FuncArgs__resultImage = resultImage;
	{
		void* VNR_vardecl_0(resultImage);
		LCAccNode VNR_vardecl_1(911, (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 0);
		LCAccNode VNR_vardecl_2(911, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 1);
		LCAccNode VNR_vardecl_3(911, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 2);
		LCAccNode VNR_vardecl_4(912, (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 3);
		LCAccNode VNR_vardecl_5(913, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 4);
		LCAccNode VNR_vardecl_6(911, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 5);
		LCAccNode VNR_vardecl_7(911, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 6);
		LCAccNode VNR_vardecl_8(912, (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 7);
		LCAccNode VNR_vardecl_9(913, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 8);
		LCAccNode VNR_vardecl_10(911, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 9);
		LCAccNode VNR_vardecl_11(911, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 10);
		LCAccNode VNR_vardecl_12(912, (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 11);
		LCAccNode VNR_vardecl_13(913, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 12);
		LCAccNode VNR_vardecl_14(911, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 13);
		LCAccNode VNR_vardecl_15(911, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 14);
		LCAccNode VNR_vardecl_16(912, (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 15);
		LCAccNode VNR_vardecl_17(913, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 16);
		LCAccNode VNR_vardecl_18(914, (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)), 3, 0, 17);
		std::vector<uint32_t> VNR_vardecl_19;
		VNR_vardecl_19.push_back(((1) - (0)) / (1));
		VNR_vardecl_19.push_back(((1) - (0)) / (1));
		VNR_vardecl_19.push_back(((chunk) - (0)) / (1));
		std::vector<int32_t> VNR_vardecl_20;
		VNR_vardecl_20.push_back((1) * (sizeof(float) * (1) * (chunk)));
		VNR_vardecl_20.push_back((1) * (sizeof(float) * (chunk)));
		VNR_vardecl_20.push_back((1) * (sizeof(float)));
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_21(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_22;
		VNR_vardecl_22.push_back(VNR_vardecl_21);
		std::vector<uint64_t> VNR_vardecl_23;
		VNR_vardecl_23.push_back(ConvertToType<uint32_t, uint64_t>((0))/*Register output0*/);
		VNR_vardecl_23.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register output1*/);
		VNR_vardecl_23.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register output2*/);
		VNR_vardecl_23.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register output3*/);
		VNR_vardecl_23.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX0*/);
		VNR_vardecl_23.push_back(ConvertToType<int32_t, uint64_t>(-2)/*Register offsetY0*/);
		VNR_vardecl_23.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX1*/);
		VNR_vardecl_23.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY1*/);
		VNR_vardecl_23.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetX2*/);
		VNR_vardecl_23.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY2*/);
		VNR_vardecl_23.push_back(ConvertToType<int32_t, uint64_t>(1)/*Register offsetX3*/);
		VNR_vardecl_23.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY3*/);
		VNR_vardecl_23.push_back(ConvertToType<uint32_t, uint64_t>(4)/*Register elementSize*/);
		VNR_vardecl_23.push_back(ConvertToType<uint32_t, uint64_t>(2)/*Register elementCount*/);
		VNR_vardecl_23.push_back(ConvertToType<intptr_t, uint64_t>(atlasArray)/*Register inputAddr*/);
		VNR_vardecl_23.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register width*/);
		VNR_vardecl_23.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register height*/);
		VNR_vardecl_23.push_back(ConvertToType<int, uint64_t>(lineNumber)/*Register line*/);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_24((0) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_25;
		VNR_vardecl_25.push_back(VNR_vardecl_24);
		std::vector<uint64_t> VNR_vardecl_26;
		VNR_vardecl_26.push_back(ConvertToType<uint32_t, uint64_t>((0))/*Register output0*/);
		VNR_vardecl_26.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk))))/*Register output1*/);
		VNR_vardecl_26.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register output2*/);
		VNR_vardecl_26.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register output3*/);
		VNR_vardecl_26.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX0*/);
		VNR_vardecl_26.push_back(ConvertToType<int32_t, uint64_t>(-2)/*Register offsetY0*/);
		VNR_vardecl_26.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX1*/);
		VNR_vardecl_26.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY1*/);
		VNR_vardecl_26.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetX2*/);
		VNR_vardecl_26.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY2*/);
		VNR_vardecl_26.push_back(ConvertToType<int32_t, uint64_t>(1)/*Register offsetX3*/);
		VNR_vardecl_26.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY3*/);
		VNR_vardecl_26.push_back(ConvertToType<uint32_t, uint64_t>(4)/*Register elementSize*/);
		VNR_vardecl_26.push_back(ConvertToType<uint32_t, uint64_t>(1)/*Register elementCount*/);
		VNR_vardecl_26.push_back(ConvertToType<intptr_t, uint64_t>(targetArrayStart)/*Register inputAddr*/);
		VNR_vardecl_26.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register width*/);
		VNR_vardecl_26.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register height*/);
		VNR_vardecl_26.push_back(ConvertToType<int, uint64_t>(lineNumber)/*Register line*/);
		std::vector<uint64_t> VNR_vardecl_27;
		VNR_vardecl_27.push_back(ConvertToType<uint32_t, uint64_t>((0))/*Register output0*/);
		VNR_vardecl_27.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk))))/*Register output1*/);
		VNR_vardecl_27.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register output2*/);
		VNR_vardecl_27.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register output3*/);
		VNR_vardecl_27.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX0*/);
		VNR_vardecl_27.push_back(ConvertToType<int32_t, uint64_t>(-2)/*Register offsetY0*/);
		VNR_vardecl_27.push_back(ConvertToType<int32_t, uint64_t>(0)/*Register offsetX1*/);
		VNR_vardecl_27.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY1*/);
		VNR_vardecl_27.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetX2*/);
		VNR_vardecl_27.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY2*/);
		VNR_vardecl_27.push_back(ConvertToType<int32_t, uint64_t>(1)/*Register offsetX3*/);
		VNR_vardecl_27.push_back(ConvertToType<int32_t, uint64_t>(-1)/*Register offsetY3*/);
		VNR_vardecl_27.push_back(ConvertToType<uint32_t, uint64_t>(4)/*Register elementSize*/);
		VNR_vardecl_27.push_back(ConvertToType<uint32_t, uint64_t>(1)/*Register elementCount*/);
		VNR_vardecl_27.push_back(ConvertToType<intptr_t, uint64_t>((uint64_t)resultImage)/*Register inputAddr*/);
		VNR_vardecl_27.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register width*/);
		VNR_vardecl_27.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register height*/);
		VNR_vardecl_27.push_back(ConvertToType<int, uint64_t>(lineNumber)/*Register line*/);
		std::vector<int32_t> VNR_vardecl_28;
		VNR_vardecl_28.push_back((1) * (sizeof(int32_t[2]) * (1) * (chunk)));
		VNR_vardecl_28.push_back((1) * (sizeof(int32_t[2]) * (chunk)));
		VNR_vardecl_28.push_back((1) * (sizeof(int32_t[2])));
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_29((0) + ((((0) * (sizeof(int32_t[2]) * (1) * (chunk))) + ((0) * (sizeof(int32_t[2]) * (chunk))) + ((0) * (sizeof(int32_t[2]))))), sizeof(int32_t), VNR_vardecl_19, VNR_vardecl_28);
		std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_30;
		VNR_vardecl_30.push_back(VNR_vardecl_29);
		std::vector<uint64_t> VNR_vardecl_31;
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(1)/*Register xBias*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(0)/*Register yBias*/);
		VNR_vardecl_31.push_back(ConvertToType<intptr_t, uint64_t>(imageArrayStart)/*Register sourceImageAddr*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(inputWidth)/*Register sourceImageWidth*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(inputHeight)/*Register sourceImageHeight*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register dstImageWidth*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register dstImageHeight*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias0*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(-2)/*Register candidateYBias0*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias1*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias1*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias2*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias2*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias3*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias3*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias4*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias4*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias5*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias5*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias6*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias6*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias7*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias7*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias8*/);
		VNR_vardecl_31.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias8*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register targetSPMLocation0*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation1*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation2*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation3*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation4*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation5*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation6*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation7*/);
		VNR_vardecl_31.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation8*/);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_32(((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_33(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_34(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_35(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_36(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_37(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_38(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_39(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_40(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_41(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_42(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_43(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_44(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_45(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_46(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		MicroprogramWriter::ComputeArgIndex VNR_vardecl_47(((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * (sizeof(float) * (1) * (chunk))) + ((0) * (sizeof(float) * (chunk))) + ((0) * (sizeof(float))))), sizeof(float), VNR_vardecl_19, VNR_vardecl_20);
		std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_48;
		VNR_vardecl_48.push_back(VNR_vardecl_24);
		VNR_vardecl_48.push_back(VNR_vardecl_32);
		VNR_vardecl_48.push_back(VNR_vardecl_33);
		VNR_vardecl_48.push_back(VNR_vardecl_34);
		VNR_vardecl_48.push_back(VNR_vardecl_35);
		VNR_vardecl_48.push_back(VNR_vardecl_36);
		VNR_vardecl_48.push_back(VNR_vardecl_37);
		VNR_vardecl_48.push_back(VNR_vardecl_38);
		VNR_vardecl_48.push_back(VNR_vardecl_39);
		VNR_vardecl_48.push_back(VNR_vardecl_40);
		VNR_vardecl_48.push_back(VNR_vardecl_41);
		VNR_vardecl_48.push_back(VNR_vardecl_42);
		VNR_vardecl_48.push_back(VNR_vardecl_43);
		VNR_vardecl_48.push_back(VNR_vardecl_44);
		VNR_vardecl_48.push_back(VNR_vardecl_45);
		VNR_vardecl_48.push_back(VNR_vardecl_46);
		VNR_vardecl_48.push_back(VNR_vardecl_47);
		std::vector<uint64_t> VNR_vardecl_49;
		std::vector<uint64_t> VNR_vardecl_50;
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(1)/*Register xBias*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(1)/*Register yBias*/);
		VNR_vardecl_50.push_back(ConvertToType<intptr_t, uint64_t>(imageArrayStart)/*Register sourceImageAddr*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(inputWidth)/*Register sourceImageWidth*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(inputHeight)/*Register sourceImageHeight*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register dstImageWidth*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register dstImageHeight*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias0*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(-2)/*Register candidateYBias0*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias1*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias1*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias2*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias2*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias3*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias3*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias4*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias4*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias5*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias5*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias6*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias6*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias7*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias7*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias8*/);
		VNR_vardecl_50.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias8*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register targetSPMLocation0*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation1*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation2*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation3*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation4*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation5*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation6*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation7*/);
		VNR_vardecl_50.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation8*/);
		std::vector<uint64_t> VNR_vardecl_51;
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(0)/*Register xBias*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(1)/*Register yBias*/);
		VNR_vardecl_51.push_back(ConvertToType<intptr_t, uint64_t>(imageArrayStart)/*Register sourceImageAddr*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(inputWidth)/*Register sourceImageWidth*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(inputHeight)/*Register sourceImageHeight*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register dstImageWidth*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register dstImageHeight*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias0*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(-2)/*Register candidateYBias0*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias1*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias1*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias2*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias2*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias3*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias3*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias4*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias4*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias5*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias5*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias6*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias6*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias7*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias7*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias8*/);
		VNR_vardecl_51.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias8*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register targetSPMLocation0*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation1*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation2*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation3*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation4*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation5*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation6*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation7*/);
		VNR_vardecl_51.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation8*/);
		std::vector<uint64_t> VNR_vardecl_52;
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(-1)/*Register xBias*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(1)/*Register yBias*/);
		VNR_vardecl_52.push_back(ConvertToType<intptr_t, uint64_t>(imageArrayStart)/*Register sourceImageAddr*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(inputWidth)/*Register sourceImageWidth*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(inputHeight)/*Register sourceImageHeight*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(outputWidth)/*Register dstImageWidth*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(outputHeight)/*Register dstImageHeight*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias0*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(-2)/*Register candidateYBias0*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias1*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias1*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias2*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias2*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias3*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateYBias3*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateXBias4*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias4*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(-1)/*Register candidateXBias5*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias5*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias6*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias6*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias7*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(1)/*Register candidateYBias7*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateXBias8*/);
		VNR_vardecl_52.push_back(ConvertToType<int, uint64_t>(0)/*Register candidateYBias8*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk))))/*Register targetSPMLocation0*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation1*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation2*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation3*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation4*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation5*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation6*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation7*/);
		VNR_vardecl_52.push_back(ConvertToType<uint32_t, uint64_t>(((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))))/*Register targetSPMLocation8*/);
		std::vector<MicroprogramWriter::ComputeArgIndex> VNR_vardecl_53;
		VNR_vardecl_53.push_back(VNR_vardecl_24);
		VNR_vardecl_53.push_back(VNR_vardecl_33);
		VNR_vardecl_53.push_back(VNR_vardecl_35);
		VNR_vardecl_53.push_back(VNR_vardecl_37);
		VNR_vardecl_53.push_back(VNR_vardecl_32);
		VNR_vardecl_53.push_back(VNR_vardecl_34);
		VNR_vardecl_53.push_back(VNR_vardecl_36);
		VNR_vardecl_53.push_back(VNR_vardecl_38);
		VNR_vardecl_53.push_back(VNR_vardecl_39);
		std::vector<uint32_t> VNR_vardecl_54;
		VNR_vardecl_54.push_back(((1) - (0)) / (1));
		VNR_vardecl_54.push_back(((1) - (0)) / (1));
		VNR_vardecl_54.push_back(((chunk) - (0)) / (1));
		VNR_vardecl_54.push_back((2));
		std::vector<int32_t> VNR_vardecl_55;
		VNR_vardecl_55.push_back((1) * ((sizeof(int32_t[2])) * (1) * (chunk)));
		VNR_vardecl_55.push_back((1) * ((sizeof(int32_t[2])) * (chunk)));
		VNR_vardecl_55.push_back((1) * ((sizeof(int32_t[2]))));
		VNR_vardecl_55.push_back((sizeof(int32_t)));
		std::vector<int32_t> VNR_vardecl_56;
		VNR_vardecl_56.push_back((1) * ((sizeof(float)) * (1) * (chunk)));
		VNR_vardecl_56.push_back((1) * ((sizeof(float)) * (chunk)));
		VNR_vardecl_56.push_back((1) * ((sizeof(float))));
		std::vector<uint32_t> VNR_vardecl_57;
		VNR_vardecl_57.push_back(((imageCount) - (0)) / (1));
		VNR_vardecl_57.push_back(((lineNumber + 1) - (lineNumber)) / (1));
		VNR_vardecl_57.push_back(((outputWidth) - (0)) / (chunk));
		std::vector<int32_t> VNR_vardecl_58;
		VNR_vardecl_58.push_back((1) * ((sizeof(float) * (outputHeight) * (outputWidth))));
		VNR_vardecl_58.push_back((1) * ((sizeof(float) * (outputWidth))));
		VNR_vardecl_58.push_back((chunk) * ((sizeof(float))));
		std::vector<int32_t> VNR_vardecl_59;
		VNR_vardecl_59.push_back((1) * ((sizeof(float) * (outputHeight) * (outputWidth))));
		VNR_vardecl_59.push_back((1) * ((sizeof(float) * (outputWidth))));
		VNR_vardecl_59.push_back((1) * ((sizeof(float))));
		//See VNR_vardecl_22 for index variable decl
		//See VNR_vardecl_23 for register set decl
		mw.AddCompute(VNR_vardecl_1, VNR_vardecl_22, VNR_vardecl_23);
		
		//See VNR_vardecl_25 for index variable decl
		//See VNR_vardecl_26 for register set decl
		mw.AddCompute(VNR_vardecl_2, VNR_vardecl_25, VNR_vardecl_26);
		
		//See VNR_vardecl_25 for index variable decl
		//See VNR_vardecl_27 for register set decl
		mw.AddCompute(VNR_vardecl_3, VNR_vardecl_25, VNR_vardecl_27);
		
		//See VNR_vardecl_30 for index variable decl
		//See VNR_vardecl_31 for register set decl
		mw.AddCompute(VNR_vardecl_4, VNR_vardecl_30, VNR_vardecl_31);
		
		//See VNR_vardecl_48 for index variable decl
		//See VNR_vardecl_49 for register set decl
		mw.AddCompute(VNR_vardecl_5, VNR_vardecl_48, VNR_vardecl_49);
		
		//See VNR_vardecl_25 for index variable decl
		//See VNR_vardecl_26 for register set decl
		mw.AddCompute(VNR_vardecl_6, VNR_vardecl_25, VNR_vardecl_26);
		
		//See VNR_vardecl_25 for index variable decl
		//See VNR_vardecl_27 for register set decl
		mw.AddCompute(VNR_vardecl_7, VNR_vardecl_25, VNR_vardecl_27);
		
		//See VNR_vardecl_30 for index variable decl
		//See VNR_vardecl_50 for register set decl
		mw.AddCompute(VNR_vardecl_8, VNR_vardecl_30, VNR_vardecl_50);
		
		//See VNR_vardecl_48 for index variable decl
		//See VNR_vardecl_49 for register set decl
		mw.AddCompute(VNR_vardecl_9, VNR_vardecl_48, VNR_vardecl_49);
		
		//See VNR_vardecl_25 for index variable decl
		//See VNR_vardecl_26 for register set decl
		mw.AddCompute(VNR_vardecl_10, VNR_vardecl_25, VNR_vardecl_26);
		
		//See VNR_vardecl_25 for index variable decl
		//See VNR_vardecl_27 for register set decl
		mw.AddCompute(VNR_vardecl_11, VNR_vardecl_25, VNR_vardecl_27);
		
		//See VNR_vardecl_30 for index variable decl
		//See VNR_vardecl_51 for register set decl
		mw.AddCompute(VNR_vardecl_12, VNR_vardecl_30, VNR_vardecl_51);
		
		//See VNR_vardecl_48 for index variable decl
		//See VNR_vardecl_49 for register set decl
		mw.AddCompute(VNR_vardecl_13, VNR_vardecl_48, VNR_vardecl_49);
		
		//See VNR_vardecl_25 for index variable decl
		//See VNR_vardecl_26 for register set decl
		mw.AddCompute(VNR_vardecl_14, VNR_vardecl_25, VNR_vardecl_26);
		
		//See VNR_vardecl_25 for index variable decl
		//See VNR_vardecl_27 for register set decl
		mw.AddCompute(VNR_vardecl_15, VNR_vardecl_25, VNR_vardecl_27);
		
		//See VNR_vardecl_30 for index variable decl
		//See VNR_vardecl_52 for register set decl
		mw.AddCompute(VNR_vardecl_16, VNR_vardecl_30, VNR_vardecl_52);
		
		//See VNR_vardecl_48 for index variable decl
		//See VNR_vardecl_49 for register set decl
		mw.AddCompute(VNR_vardecl_17, VNR_vardecl_48, VNR_vardecl_49);
		
		//See VNR_vardecl_53 for index variable decl
		//See VNR_vardecl_49 for register set decl
		mw.AddCompute(VNR_vardecl_18, VNR_vardecl_53, VNR_vardecl_49);
		
		//transfer from atlasReader to regionSample0
		//Search VNR_vardecl_54 for source size.
		//Search VNR_vardecl_55 for source stride.
		//Search VNR_vardecl_54 for destination size.
		//Search VNR_vardecl_55 for destination stride.
		mw.AddTransfer(VNR_vardecl_1, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_54, VNR_vardecl_55, VNR_vardecl_4, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_54, VNR_vardecl_55, sizeof(int32_t));
		
		//transfer from atlasReader to regionSample1
		//Search VNR_vardecl_54 for source size.
		//Search VNR_vardecl_55 for source stride.
		//Search VNR_vardecl_54 for destination size.
		//Search VNR_vardecl_55 for destination stride.
		mw.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_54, VNR_vardecl_55, VNR_vardecl_8, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_54, VNR_vardecl_55, sizeof(int32_t));
		
		//transfer from atlasReader to regionSample2
		//Search VNR_vardecl_54 for source size.
		//Search VNR_vardecl_55 for source stride.
		//Search VNR_vardecl_54 for destination size.
		//Search VNR_vardecl_55 for destination stride.
		mw.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_54, VNR_vardecl_55, VNR_vardecl_12, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_54, VNR_vardecl_55, sizeof(int32_t));
		
		//transfer from atlasReader to regionSample3
		//Search VNR_vardecl_54 for source size.
		//Search VNR_vardecl_55 for source stride.
		//Search VNR_vardecl_54 for destination size.
		//Search VNR_vardecl_55 for destination stride.
		mw.AddTransfer(VNR_vardecl_1, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_54, VNR_vardecl_55, VNR_vardecl_16, (0) + ((((0) * ((sizeof(int32_t[2])) * (1) * (chunk))) + ((0) * ((sizeof(int32_t[2])) * (chunk))) + ((0) * ((sizeof(int32_t[2])))))), VNR_vardecl_54, VNR_vardecl_55, sizeof(int32_t));
		
		//transfer from targetReader0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_2, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_2, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_2, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_2, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_3, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_3, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_3, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_3, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to diffCalc0
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample0 to pickCandidate
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_4, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from diffCalc0 to pickCandidate
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_5, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_18, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_6, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_6, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_6, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_6, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_7, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_7, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_7, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_7, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to diffCalc1
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample1 to pickCandidate
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_8, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from diffCalc1 to pickCandidate
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_9, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_10, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_10, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_10, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_10, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_11, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_11, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_11, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_11, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to diffCalc2
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample2 to pickCandidate
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_12, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from diffCalc2 to pickCandidate
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_13, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_14, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_14, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_14, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from targetReader3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_14, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_15, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_15, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_15, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from resultReader3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_15, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, (0) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to diffCalc3
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from regionSample3 to pickCandidate
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_16, ((sizeof(int32_t[2]) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from diffCalc3 to pickCandidate
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_19 for destination size.
		//Search VNR_vardecl_56 for destination stride.
		mw.AddTransfer(VNR_vardecl_17, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, sizeof(float));
		
		//transfer from pickCandidate to resultImage
		//Search VNR_vardecl_19 for source size.
		//Search VNR_vardecl_56 for source stride.
		//Search VNR_vardecl_57 for destination block size.
		//Search VNR_vardecl_58 for destination block stride.
		//Search VNR_vardecl_19 for destination element size.
		//Search VNR_vardecl_59 for destination element stride.
		mw.AddTransfer(VNR_vardecl_18, ((sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk)) + (sizeof(float) * (1) * (1) * (chunk))) + ((((0) * ((sizeof(float)) * (1) * (chunk))) + ((0) * ((sizeof(float)) * (chunk))) + ((0) * ((sizeof(float)))))), VNR_vardecl_19, VNR_vardecl_56, VNR_vardecl_0, VNR_vardecl_57, VNR_vardecl_58, VNR_vardecl_19, VNR_vardecl_59, sizeof(float));
		
		mw.SetTaskGrain(0);
		mw.Finalize((((imageCount) - (0)) / (1)) * (((lineNumber + 1) - (lineNumber)) / (1)) * (((outputWidth) - (0)) / (chunk)));
	}
	*buffer = new uint8_t[mw.GetBufferSize()];
	*bufferSize = mw.GetBufferSize();
	memcpy(*buffer, mw.GetBuffer(), mw.GetBufferSize());
	Touch(thread, *buffer, *bufferSize);
}
inline void TexSynth2LCacc_td_buf(uint8_t* buf, uint32_t bufSize, int thread)
{
	InterruptArgs isrArgs;
	isrArgs.threadID = thread;
	isrArgs.lcaccID = 0;
	isrArgs.lcaccMode = 0;
	LWI_RegisterInterruptHandler(&isrArgs);
	LCAcc_Command(thread, isrArgs.lcaccID, LCACC_CMD_BEGIN_PROGRAM, buf, bufSize, 0, 0);
	Wait_td__TexSynth2LCacc(thread);// wait for everything to finish
}
void TexSynth2LCacc_td(int thread, float (*resultImage), int inputHeight, int inputWidth, int outputHeight, int outputWidth, int imageCount, intptr_t imageArrayStart, intptr_t targetArrayStart, intptr_t atlasArray, uint32_t lineNumber, int chunk)
{
	uint32_t bufSize;
	uint8_t* buffer;
	uint8_t* constCluster = NULL;
	CreateBuffer_TexSynth2LCacc_td(&buffer, &bufSize, &constCluster, thread, resultImage, inputHeight, inputWidth, outputHeight, outputWidth, imageCount, imageArrayStart, targetArrayStart, atlasArray, lineNumber, chunk);
	TexSynth2LCacc_td_buf(buffer, bufSize, thread);
	if(constCluster)
	{
		delete [] constCluster;
	}
	delete [] buffer;
}
inline uint32_t TexSynth2LCacc_CalculateBiNSize(int inputHeight, int inputWidth, int outputHeight, int outputWidth, int imageCount, intptr_t imageArrayStart, intptr_t targetArrayStart, intptr_t atlasArray, uint32_t lineNumber, int chunk)
{
	return 0;
}
class BiN_TexSynth2LCacc_Arbitrator_td
{
	std::vector<uint8_t*> bufSet;
	std::vector<uint32_t> bufSizeSet;
	std::vector<uint32_t> binSizeSet;
	std::vector<uint32_t> performancePoint;
	std::vector<uint32_t> cachePressureMod;
	InterruptArgs isr;
public:
	inline BiN_TexSynth2LCacc_Arbitrator_td(){}
	inline void AddConfig(uint8_t* buf, uint32_t bufSize, uint32_t binSize, uint32_t performance, uint32_t cacheMod)
	{
		bufSet.push_back(buf);
		bufSizeSet.push_back(bufSize);
		binSizeSet.push_back(binSize);
		performancePoint.push_back(performance);
		cachePressureMod.push_back(cacheMod);
	}
	inline void Run(int threadID)
	{
		isr.threadID = threadID;
		isr.lcaccID = 0;
		isr.lcaccMode = 0;
		LCAcc_DeclareLCAccUse(threadID, 911, 9); //requests for TexSynth2
		LCAcc_DeclareLCAccUse(threadID, 912, 4); //requests for TexSynth3
		LCAcc_DeclareLCAccUse(threadID, 913, 4); //requests for TexSynth4
		LCAcc_DeclareLCAccUse(threadID, 914, 1); //requests for TexSynth5
		for(size_t i = 0; i < binSizeSet.size(); i++)
		{
			LCAcc_SendBiNCurve(threadID, binSizeSet[i], performancePoint[i], cachePressureMod[i]);
		}
		LCAcc_SendBiNCurve(threadID, 0, 0, 0);
		LWI_RegisterInterruptHandler(&isr);
		bool cont = true;
		uint32_t bufferSize = 0;
		InterruptArgs* args = 0;
		while((args = LWI_CheckInterrupt(threadID)) == 0);
		simics_assert(args == &isr);
		switch(args->status)
		{
			case(BIN_CMD_ARBITRATE_RESPONSE):
				{
					bufferSize = args->v[0];
					cont = false;
				}
				break;
			default:
				simics_assert(0);
		}
		LWI_ClearInterrupt(threadID);
		for(size_t i = 0; i < binSizeSet.size(); i++)
		{
			if(binSizeSet[i] == bufferSize)
			{
				TexSynth2LCacc_td_buf(bufSet[i], bufSizeSet[i], threadID);
				return;
			}
		}
		simics_assert(0);
	}
};

#endif
