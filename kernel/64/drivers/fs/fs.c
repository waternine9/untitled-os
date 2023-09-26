#include "fs.h"
#include "../nvme/nvme.h"
#include "../../vmalloc.h"

#define FS_DIRFLAG_SHIFT 0
#define FS_DIRFLAG_MASK 0b1ULL
#define FS_DIRFLAG_ISDIR 1ULL
#define FS_DIRFLAG_ISFILE 0ULL

#define FS_NEXTBLOCK_SHIFT 1
#define FS_NEXTBLOCK_MASK 0xFFFFFFFFULL
#define FS_NEXTBLOCK_END 0ULL

#define FS_VALID_SHIFT 63
#define FS_VALID_MASK 0b1ULL
#define FS_VALID_ISVALID 1ULL

#define FS_NAME_STARTOFFSET 256

#define FS_ENTRY_VALID_SHIFT 0
#define FS_ENTRY_VALID_MASK 0b1ULL
#define FS_ENTRY_MYBLOCK_SHIFT 1ULL
#define FS_ENTRY_MYBLOCK_MASK 0xFFFFFFFFULL

typedef struct
{
	bool IsDir;
	bool Valid;
	char* Name;
	uint32_t NextBlock;
	uint32_t DataLBA;
} FSBlockHeader;

const char* FSSignature = "FMT_THEOS"; // 9 letters
const char* FSRootName = "root"; // 9 letters

bool FSIsFormatted()
{
	char* FirstData = AllocPhys(0x1000);
	NVMERead(1, 0, FirstData);
	for (int i = 0;i < 9;i++)
	{
		if (FirstData[i] != FSSignature[i]) 
		{
			FreeVM(FirstData);
			return false;
		}
	}
	FreeVM(FirstData);
	return true;
}

void FSFormat()
{
	char* FirstData = AllocPhys(0x1000);
	for (int i = 0;i < 1024;i++)
	{
		FirstData[i] = 0;
	}
	for (int i = 0;i < 9;i++)
	{
		FirstData[i] = FSSignature[i];
	}
	NVMEWrite(1, 0, FirstData);
	for (int i = 0;i < 1024;i++)
	{
		FirstData[i] = 0;
	}
	for (int i = 0;i < 1280;i++)
	{
		NVMEWrite(1, i, FirstData);
	}
	for (int i = 256;i < 256 + 4;i++)
	{
		FirstData[i] = FSRootName[i - 256];
	}
 	*(uint64_t*)FirstData |= FS_DIRFLAG_ISDIR << FS_DIRFLAG_SHIFT;
	*(uint64_t*)FirstData |= FS_VALID_ISVALID << FS_VALID_SHIFT;
	NVMEWrite(2, 1, FirstData);
}

void FSTryFormat()
{
	if (!FSIsFormatted())
	{
		FSFormat();
	}
}

FSBlockHeader FSQueryHeader(int Block)
{
	uint64_t* FirstData = AllocPhys(0x1000);
	NVMERead(1, 1 + Block * 2, FirstData);
	FSBlockHeader Header;
	Header.IsDir = (*FirstData >> FS_DIRFLAG_SHIFT) & FS_DIRFLAG_MASK;
	Header.NextBlock = (*FirstData >> FS_NEXTBLOCK_SHIFT) & FS_NEXTBLOCK_MASK;
	Header.DataLBA = 1 + Block * 2 + 1;
	Header.Valid = (*FirstData >> FS_VALID_SHIFT) & FS_VALID_MASK;
	if (Header.Valid)
	{
		Header.Name = AllocPhys(256);
		Header.Name[255] = 0;
		for (int i = 256;i < 511;i++)
		{
			Header.Name[i - 256] = ((char*)FirstData)[i];
		}
	}
	else
	{
		Header.Name = 0;
	}
	FreeVM(FirstData);
	return Header;
}

void FSFormatBlock(int Block)
{
	uint8_t FirstData[512];
	for (int i = 0;i < 512;i++)
	{
		FirstData[i] = 0;
	}
	NVMEWrite(1, 1 + Block * 2 + 1, FirstData);
}

void FSSetHeader(int Block, FSBlockHeader Header)
{
	uint8_t* FirstData = AllocPhys(512);
	for (int i = 0;i < 512;i++)
	{
		FirstData[i] = 0;
	}
	if (Header.IsDir)
	{
		*(uint64_t*)FirstData |= 1ULL << FS_DIRFLAG_SHIFT;
	}
	*(uint64_t*)FirstData |= (uint64_t)Header.NextBlock << FS_NEXTBLOCK_SHIFT;
	if (Header.Valid)
	{
		*(uint64_t*)FirstData |= 1ULL << FS_VALID_SHIFT;
	}
	if (Header.Name)
	{
		for (int i = 0;Header.Name[i] && i < 255;i++)
		{
			FirstData[i + 256] = Header.Name[i];
		}
	}
	NVMEWrite(1, 1 + Block * 2, FirstData);
}

bool FSStrcmp(char* x, char* y)
{
	int xlen = 0, ylen = 0;
	while (x[xlen]) xlen++;
	while (y[ylen]) ylen++;

	if (xlen != ylen) return false;

	for (int i = 0;i < xlen;i++)
	{
		if (x[i] != y[i]) return false;
	}
	return true;
}

uint64_t FSGetDirInDir(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	NVMERead(1, CurHeader.DataLBA, Entries);

	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if ((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK)
		{
			FSBlockHeader EntryHeader = FSQueryHeader((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
			if (!EntryHeader.IsDir) 
			{
				if (EntryHeader.Name) FreeVM(EntryHeader.Name);
				continue;
			}
			if (FSStrcmp(EntryHeader.Name, Name))
			{
				if (EntryHeader.Name) FreeVM(EntryHeader.Name);
				return (Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK;
			}
			if (EntryHeader.Name) FreeVM(EntryHeader.Name);
		}
	}

	uint64_t NextBlock = Entries[256 / 8 - 1];
	FreeVM(Entries);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	if ((NextBlock >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) return FSGetDirInDir((NextBlock >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK, Name);
	return 0;
}

uint64_t FSGetAnyInDir(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	NVMERead(1, CurHeader.DataLBA, Entries);

	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if ((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK)
		{
			FSBlockHeader EntryHeader = FSQueryHeader((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
			if (EntryHeader.Name) 
			{
				if (FSStrcmp(EntryHeader.Name, Name))
				{
					uint64_t NextBlock = Entries[i];
					FreeVM(Entries);
					if (EntryHeader.Name) FreeVM(EntryHeader.Name);
					return (NextBlock >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK;
				}
				FreeVM(EntryHeader.Name);
			}
		}
	}


	uint64_t NextBlock = Entries[256 / 8 - 1];
	FreeVM(Entries);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	if ((NextBlock >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) return FSGetAnyInDir((NextBlock >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK, Name);
	return 0;
}

uint64_t FSAllocBlock()
{
	for (uint64_t i = 1;i < 0xFFFFFFFF;i++)
	{
		FSBlockHeader CurHeader = FSQueryHeader(i);
		if (!CurHeader.Valid) 
		{
			return i;
		}
	}
	return 0;
}

void FSMkdirAt(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	NVMERead(1, CurHeader.DataLBA, Entries);
	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if (((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) == 0)
		{
			Entries[i] |= 1ULL << FS_ENTRY_VALID_SHIFT;
			uint64_t Block = FSAllocBlock();
			if (Block == 0) return;
			Entries[i] |= Block << FS_ENTRY_MYBLOCK_SHIFT;
			NVMEWrite(1, CurHeader.DataLBA, Entries);
			FSBlockHeader Header;
			Header.Name = Name;
			Header.IsDir = true;
			Header.NextBlock = 0;
			Header.Valid = true;
			FSSetHeader(Block, Header);

			FreeVM(Entries);
			if (CurHeader.Name) FreeVM(CurHeader.Name);

			return;
		}
	}
	
	uint64_t NextBlock = Entries[256 / 8 - 1];
	FreeVM(Entries);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	if ((NextBlock >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) FSMkdirAt((NextBlock >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK, Name);
	else
	{
		uint64_t NewBlock = FSAllocBlock();

		FSBlockHeader Header;
		Header.Name = Name;
		Header.IsDir = true;
		Header.NextBlock = 0;
		Header.Valid = true;
		FSFormatBlock(NewBlock);
		FSSetHeader(NewBlock, Header);
		FSMkdirAt(NewBlock, Name);
		Entries[256 / 8 - 1] |= 1ULL << FS_ENTRY_VALID_SHIFT;
		Entries[256 / 8 - 1] |= NewBlock << FS_ENTRY_MYBLOCK_SHIFT;
		NVMEWrite(1, CurHeader.DataLBA, Entries);
	}
}

void FSMkfileAt(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	NVMERead(1, CurHeader.DataLBA, Entries);
	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if (((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) == 0)
		{
			Entries[i] |= 1ULL << FS_ENTRY_VALID_SHIFT;
			uint64_t Block = FSAllocBlock();
			if (Block == 0) return;
			Entries[i] |= Block << FS_ENTRY_MYBLOCK_SHIFT;
			NVMEWrite(1, CurHeader.DataLBA, Entries);
			FSBlockHeader Header;
			Header.Name = Name;
			Header.IsDir = false;
			Header.NextBlock = 0;
			Header.Valid = true;
			FSSetHeader(Block, Header);

			FreeVM(Entries);
			if (CurHeader.Name) FreeVM(CurHeader.Name);

			return;
		}
	}
	
	uint64_t NextBlock = Entries[256 / 8 - 1];
	FreeVM(Entries);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	if ((NextBlock >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) FSMkfileAt((NextBlock >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK, Name);
	else
	{
		uint64_t NewBlock = FSAllocBlock();

		FSBlockHeader Header;
		Header.Name = Name;
		Header.IsDir = true;
		Header.NextBlock = 0;
		Header.Valid = true;
		FSFormatBlock(NewBlock);
		FSSetHeader(NewBlock, Header);
		FSMkfileAt(NewBlock, Name);
		Entries[256 / 8 - 1] |= 1ULL << FS_ENTRY_VALID_SHIFT;
		Entries[256 / 8 - 1] |= NewBlock << FS_ENTRY_MYBLOCK_SHIFT;
		NVMEWrite(1, CurHeader.DataLBA, Entries);
	}
}

void FSRemoveAtCleanUpFile(uint64_t Block)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (!CurHeader.Valid) return;
	if (CurHeader.IsDir) return;
	size_t ReadBytes = 0;
	while (true)
	{
		CurHeader.Valid = false;
		FSSetHeader(Block, CurHeader);
		if (CurHeader.NextBlock == 0) break;
		Block = CurHeader.NextBlock;
		if (CurHeader.Name) FreeVM(CurHeader.Name);
		CurHeader = FSQueryHeader(Block);
	}
	if (CurHeader.Name) FreeVM(CurHeader.Name);
}

void FSRemoveAtCleanUp(uint64_t Block)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	uint64_t* Entries = AllocPhys(512);
	NVMERead(1, CurHeader.DataLBA, Entries);
	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if ((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK)
		{
			FSBlockHeader Header = FSQueryHeader((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
			if (Header.Name) FreeVM(Header.Name);
			if (Header.IsDir)
			{
				FSRemoveAtCleanUp((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
			}
			else
			{
				FSRemoveAtCleanUpFile((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
			}
			Entries[i] &= ~(1ULL << FS_ENTRY_VALID_SHIFT);
		}
	}
	
	uint64_t NextBlock = Entries[256 / 8 - 1];
	if ((NextBlock >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) 
	{
		FSRemoveAtCleanUp((NextBlock >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
	}

	NVMEWrite(1, CurHeader.DataLBA, Entries);
	FreeVM(Entries);
}

void FSRemoveAt(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	NVMERead(1, CurHeader.DataLBA, Entries);
	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if ((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK)
		{
			FSBlockHeader Header = FSQueryHeader((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
			if (FSStrcmp(Header.Name, Name))
			{
				if (Header.IsDir) FSRemoveAtCleanUp((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK); 
				else FSRemoveAtCleanUpFile((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK); 
				Entries[i] &= ~(1ULL << FS_ENTRY_VALID_SHIFT);
				if (Header.Name) FreeVM(Header.Name);
				if (CurHeader.Name) FreeVM(CurHeader.Name);
				NVMEWrite(1, CurHeader.DataLBA, Entries);
				if (Header.Name) FreeVM(Header.Name);
				return;
			}
			if (Header.Name) FreeVM(Header.Name);
		}
	}
	
	uint64_t NextBlock = Entries[256 / 8 - 1];
	FreeVM(Entries);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	if ((NextBlock >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) FSRemoveAt((NextBlock >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK, Name);
}

void FSWriteFileAt(uint64_t Block, char* Name, void* Data, size_t Bytes)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (!CurHeader.Valid) return;
	if (CurHeader.IsDir) return;
	if (Bytes == 0) return;
	while (true)
	{
		NVMEWrite(1, CurHeader.DataLBA, Data);
		int Sectors = Bytes / 512;
		
		if (Sectors == 0) break;
		
		Bytes -= 512;
		Data += 512;

		if (CurHeader.NextBlock > 0) Block = CurHeader.NextBlock;
		else
		{
			CurHeader.NextBlock = FSAllocBlock();
			FSBlockHeader Header;
			Header.Valid = true;
			Header.NextBlock = 0;
			Header.Name = Name;
			Header.IsDir = false;
			FSSetHeader(CurHeader.NextBlock, Header);
			FSSetHeader(Block, CurHeader);
			Block = CurHeader.NextBlock;
		}

		if (CurHeader.Name) FreeVM(CurHeader.Name);
		CurHeader = FSQueryHeader(Block);
	}
	if (CurHeader.Name) FreeVM(CurHeader.Name);
}


size_t FSReadFileAt(uint64_t Block, void* Data, size_t Bytes)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (!CurHeader.Valid) return 0;
	if (CurHeader.IsDir) return 0;
	if (Bytes == 0) return 0;
	size_t ReadBytes = 0;
	while (true)
	{
		NVMERead(1, CurHeader.DataLBA, Data);
		int Sectors = Bytes / 512;
		
		if (Sectors == 0) break;
		
		Bytes -= 512;
		ReadBytes += 512;
		Data += 512;

		if (CurHeader.NextBlock == 0) break;
		Block = CurHeader.NextBlock;

		if (CurHeader.Name) FreeVM(CurHeader.Name);
		CurHeader = FSQueryHeader(Block);
	}
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	return ReadBytes;
}

size_t FSFileSizeAt(uint64_t Block)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (!CurHeader.Valid) return 0;
	if (CurHeader.IsDir) return 0;
	size_t OutBytes = 0;
	while (true)
	{
		OutBytes += 512;
		if (CurHeader.NextBlock == 0) break;
		if (CurHeader.Name) FreeVM(CurHeader.Name);
		CurHeader = FSQueryHeader(Block);
	}
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	return OutBytes;
}

size_t FSDirectorySizeAt(uint64_t Block)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (!CurHeader.Valid) return 0;
	uint64_t* Entries = AllocPhys(512);
	NVMERead(1, CurHeader.DataLBA, Entries);
	size_t NumEntries = 0;
	for (int i = 0;i < 256 / 8 - 1;i++)
	{
		if ((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) NumEntries++;
	}
	if ((Entries[256 / 8 - 1] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK)
	{
		NumEntries += FSDirectorySizeAt((Entries[256 / 8 - 1] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);  
	}
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	return NumEntries;
}

void FSListFilesAt(uint64_t Block, FileListEntry* List)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (!CurHeader.Valid) return;
	uint64_t* Entries = AllocPhys(512);
	NVMERead(1, CurHeader.DataLBA, Entries);
	size_t NumEntries = 0;
	for (int i = 0;i < 256 / 8 - 1;i++)
	{
		if ((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK)
		{
			FSBlockHeader Header = FSQueryHeader((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
			List[NumEntries].IsDir = Header.IsDir;
			List[NumEntries].Name = AllocPhys(256);
			for (int j = 0;j < 256;j++) List[NumEntries].Name[j] = 0;
			for (int j = 0;Header.Name[j];j++) List[NumEntries].Name[j] = Header.Name[j];
			NumEntries++;
		}
	}
	if ((Entries[256 / 8 - 1] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK)
	{
		FSListFilesAt((Entries[256 / 8 - 1] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK, List + NumEntries);  
	}
	FreeVM(Entries);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
}

bool FSMkdir(char* Dir)
{
	char TempBuf[256];
	for (int i = 0;i < 256;i++)
	{
		TempBuf[i] = 0;
	}
	int i = 0;
	int CurrentBlock = 0;
	while (true)
	{
		if (Dir[i] == '/')
		{
			i++;

			continue;
		}

		for (int j = 0;j < 256;j++)
		{
			TempBuf[j] = 0;
		}
		for (int j = 0;Dir[i] != '/' && Dir[i];j++, i++)
		{
			TempBuf[j] = Dir[i];
		}

		bool End = false;
		if (Dir[i] == '/' && Dir[i + 1] == 0) End = true;
		if (Dir[i] == 0) End = true;

		if (!End)
		{
			uint64_t NextBlock = FSGetDirInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return false; // Dir doesnt exist
			CurrentBlock = NextBlock;
		}
		else
		{
			uint64_t NextBlock = FSGetAnyInDir(CurrentBlock, TempBuf);
			if (NextBlock > 0) return false; // File or dir with same name already exists
			FSMkdirAt(CurrentBlock, TempBuf);
			return true;
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}

bool FSMkfile(char* File)
{
	char TempBuf[256];
	for (int i = 0;i < 256;i++)
	{
		TempBuf[i] = 0;
	}
	int i = 0;
	int CurrentBlock = 0;
	while (true)
	{
		if (File[i] == '/')
		{
			i++;

			continue;
		}

		for (int j = 0;j < 256;j++)
		{
			TempBuf[j] = 0;
		}
		for (int j = 0;File[i] != '/' && File[i];j++, i++)
		{
			TempBuf[j] = File[i];
		}

		bool End = false;
		if (File[i] == '/' && File[i + 1] == 0) End = true;
		if (File[i] == 0) End = true;

		if (!End)
		{
			uint64_t NextBlock = FSGetDirInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return false; // Dir doesnt exist
			CurrentBlock = NextBlock;
		}
		else
		{
			uint64_t NextBlock = FSGetAnyInDir(CurrentBlock, TempBuf);
			if (NextBlock > 0) return false; // File or dir with same name already exists
			FSMkfileAt(CurrentBlock, TempBuf);
			return true;
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}

bool FSRemove(char* Any)
{
	char TempBuf[256];
	for (int i = 0;i < 256;i++)
	{
		TempBuf[i] = 0;
	}
	int i = 0;
	int CurrentBlock = 0;
	while (true)
	{
		if (Any[i] == '/')
		{
			i++;

			continue;
		}

		for (int j = 0;j < 256;j++)
		{
			TempBuf[j] = 0;
		}
		for (int j = 0;Any[i] != '/' && Any[i];j++, i++)
		{
			TempBuf[j] = Any[i];
		}

		bool End = false;
		if (Any[i] == '/' && Any[i + 1] == 0) End = true;
		if (Any[i] == 0) End = true;

		if (!End)
		{
			uint64_t NextBlock = FSGetDirInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return false; // Dir doesnt exist
			CurrentBlock = NextBlock;
		}
		else
		{
			FSRemoveAt(CurrentBlock, TempBuf);
			return true;
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}
bool FSWriteFile(char* File, void* Data, size_t Bytes)
{
	char TempBuf[256];
	for (int i = 0;i < 256;i++)
	{
		TempBuf[i] = 0;
	}
	int i = 0;
	int CurrentBlock = 0;
	while (true)
	{
		if (File[i] == '/')
		{
			i++;

			continue;
		}

		for (int j = 0;j < 256;j++)
		{
			TempBuf[j] = 0;
		}
		for (int j = 0;File[i] != '/' && File[i];j++, i++)
		{
			TempBuf[j] = File[i];
		}

		bool End = false;
		if (File[i] == '/' && File[i + 1] == 0) End = true;
		if (File[i] == 0) End = true;

		if (!End)
		{
			uint64_t NextBlock = FSGetDirInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return false; // Dir doesnt exist
			CurrentBlock = NextBlock;
		}
		else
		{
			uint64_t NextBlock = FSGetAnyInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return false; // File doesnt exist
			size_t FileSize = FSFileSizeAt(CurrentBlock);
			FSWriteFileAt(NextBlock, TempBuf, Data, Bytes);
			return true;
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}

void* FSReadFile(char* File, size_t* BytesRead)
{
	char TempBuf[256];
	for (int i = 0;i < 256;i++)
	{
		TempBuf[i] = 0;
	}
	int i = 0;
	int CurrentBlock = 0;
	while (true)
	{
		if (File[i] == '/')
		{
			i++;
			continue;
		}

		for (int j = 0;j < 256;j++)
		{
			TempBuf[j] = 0;
		}
		for (int j = 0;File[i] != '/' && File[i];j++, i++)
		{
			TempBuf[j] = File[i];
		}

		bool End = false;
		if (File[i] == '/' && File[i + 1] == 0) End = true;
		if (File[i] == 0) End = true;

		if (!End)
		{
			uint64_t NextBlock = FSGetDirInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return false; // Dir doesnt exist
			CurrentBlock = NextBlock;
		}
		else
		{
			uint64_t NextBlock = FSGetAnyInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return false; // File doesnt exist
			size_t FileSize = FSFileSizeAt(CurrentBlock);
			void* Out = AllocPhys(FileSize);
			*BytesRead = FSReadFileAt(NextBlock, Out, FileSize);
			return true;
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}

size_t FSFileSize(char* File)
{
	char TempBuf[256];
	for (int i = 0;i < 256;i++)
	{
		TempBuf[i] = 0;
	}
	int i = 0;
	int CurrentBlock = 0;
	while (true)
	{
		if (File[i] == '/')
		{
			i++;
			continue;
		}

		for (int j = 0;j < 256;j++)
		{
			TempBuf[j] = 0;
		}
		for (int j = 0;File[i] != '/' && File[i];j++, i++)
		{
			TempBuf[j] = File[i];
		}

		bool End = false;
		if (File[i] == '/' && File[i + 1] == 0) End = true;
		if (File[i] == 0) End = true;

		if (!End)
		{
			uint64_t NextBlock = FSGetDirInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return 0; // Dir doesnt exist
			CurrentBlock = NextBlock;
		}
		else
		{
			uint64_t NextBlock = FSGetAnyInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return 0; // File doesnt exist
			return FSFileSizeAt(CurrentBlock);
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}

FileListEntry* FSListFiles(char* Dir, size_t* NumEntries)
{
	char TempBuf[256];
	for (int i = 0;i < 256;i++)
	{
		TempBuf[i] = 0;
	}
	int i = 0;
	int CurrentBlock = 0;
	while (true)
	{
		if (Dir[i] == '/')
		{
			i++;
			continue;
		}

		for (int j = 0;j < 256;j++)
		{
			TempBuf[j] = 0;
		}
		for (int j = 0;Dir[i] != '/' && Dir[i];j++, i++)
		{
			TempBuf[j] = Dir[i];
		}

		bool End = false;
		if (Dir[i] == '/' && Dir[i + 1] == 0) End = true;
		if (Dir[i] == 0) End = true;

		if (!End)
		{
			uint64_t NextBlock = FSGetDirInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return 0; // Dir doesnt exist
			CurrentBlock = NextBlock;
		}
		else
		{
			uint64_t NextBlock = CurrentBlock;
			if (TempBuf[0])
			{
				NextBlock = FSGetAnyInDir(CurrentBlock, TempBuf);
				if (NextBlock == 0) return 0; // Dir doesnt exist
			}
			*NumEntries = FSDirectorySizeAt(NextBlock);
			FileListEntry* Out = AllocVM(*NumEntries * sizeof(FileListEntry));
			FSListFilesAt(NextBlock, Out);
			return Out;
		}

		for (int j = 0;j < 256;j++)
		{
			TempBuf[j] = 0;
		}
		
		i++;
	}
}