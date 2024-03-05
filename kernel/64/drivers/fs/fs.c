#include "fs.h"
#include "../driverman.h"
#include "../../vmalloc.h"
#include "../../panic.h"

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

static DriverMan_StorageDevice* FSDevice;

static void FS_DriveRead(size_t Num, size_t LBA, void* Dest)
{
	DriverMan_StorageReadFromDevice(FSDevice, Num, LBA, Dest);
}

static void FS_DriveWrite(size_t Num, size_t LBA, void* Dest)
{
	DriverMan_StorageWriteToDevice(FSDevice, Num, LBA, Dest);
}

typedef struct
{
	bool IsDir;
	bool Valid;
	char* Name;
	uint32_t NextBlock;
	uint32_t DataLBA;
} FSBlockHeader;

static const char* FSSignature = "FMT_THEOS"; // 9 letters
static const char* FSRootName = "root"; // 9 letters

static bool FSIsFormatted()
{
	char* FirstData = AllocPhys(0x1000);
	FS_DriveRead(1, 0, FirstData);
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

static void FSFormat()
{
	char* FirstData = AllocPhys(0x1000);
	for (int i = 0;i < 4096;i++)
	{
		FirstData[i] = 0;
	}
	for (int i = 0;i < 9;i++)
	{
		FirstData[i] = FSSignature[i];
	}
	FS_DriveWrite(1, 0, FirstData);
	for (int i = 0;i < 4096;i++)
	{
		FirstData[i] = 0;
	}
	for (int i = 2;i < 0x1000;i += 8)
	{
		FS_DriveWrite(8, i, FirstData);
	}
	for (int i = 256;i < 256 + 4;i++)
	{
		FirstData[i] = FSRootName[i - 256];
	}
 	*(uint64_t*)FirstData |= FS_DIRFLAG_ISDIR << FS_DIRFLAG_SHIFT;
	*(uint64_t*)FirstData |= FS_VALID_ISVALID << FS_VALID_SHIFT;
	FS_DriveWrite(2, 4, FirstData);
}

static bool FSTryFormat()
{
	if (!FSIsFormatted())
	{
		FSFormat();
		return true;
	}
	return false;
}

static FSBlockHeader FSQueryHeader(int Block)
{
	uint64_t* FirstData = AllocPhys(0x1000);
	FS_DriveRead(1, 4 + Block * 2, FirstData);
	FSBlockHeader Header;
	Header.IsDir = (*FirstData >> FS_DIRFLAG_SHIFT) & FS_DIRFLAG_MASK;
	Header.NextBlock = (*FirstData >> FS_NEXTBLOCK_SHIFT) & FS_NEXTBLOCK_MASK;
	Header.DataLBA = 4 + Block * 2 + 1;
	Header.Valid = (*FirstData >> FS_VALID_SHIFT) & FS_VALID_MASK;
	if (((char*)FirstData)[256])
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

static void FSFormatBlock(uint64_t Block)
{
	char* FirstData = AllocVM(0x1000);
	for (int i = 0;i < 0x1000;i++)
	{
		FirstData[i] = 0;
	}
	FS_DriveWrite(2, 4 + Block * 2, FirstData);
	FreeVM(FirstData);
}

static void FSSetHeader(int Block, FSBlockHeader Header)
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
	FS_DriveWrite(1, 4 + Block * 2, FirstData);
}

static bool FSStrcmp(char* x, char* y)
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

static uint64_t FSGetDirInDir(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);

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

static uint64_t FSGetAnyInDir(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);

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

static uint64_t FSGetFileInDir(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);

	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if ((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK)
		{
			FSBlockHeader EntryHeader = FSQueryHeader((Entries[i] >> FS_ENTRY_MYBLOCK_SHIFT) & FS_ENTRY_MYBLOCK_MASK);
			if (EntryHeader.Name) 
			{
				if (EntryHeader.IsDir) 
				{
					if (EntryHeader.Name) FreeVM(EntryHeader.Name);
					continue;
				}
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

static uint64_t FSAllocBlock()
{
	for (uint64_t i = 4;i < 0xFFFFFFFFULL;i += 2)
	{
		FSBlockHeader CurHeader = FSQueryHeader((i - 4) / 2);
		if (!CurHeader.Valid)
		{
			return (i - 4) / 2;
		}
	}
	return 0;
}

static void FSMkdirAt(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);
	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if (((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) == 0)
		{
			Entries[i] |= 1ULL << FS_ENTRY_VALID_SHIFT;
			uint64_t Block = FSAllocBlock();
			if (Block == 0) return;
			FSFormatBlock(Block);
			Entries[i] |= Block << FS_ENTRY_MYBLOCK_SHIFT;
			FS_DriveWrite(1, CurHeader.DataLBA, Entries);
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
		FS_DriveWrite(1, CurHeader.DataLBA, Entries);
	}
}

static void FSMkfileAt(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);
	for (int i = 0; i < 256 / 8 - 1;i++)
	{
		if (((Entries[i] >> FS_ENTRY_VALID_SHIFT) & FS_ENTRY_VALID_MASK) == 0)
		{
			Entries[i] |= 1ULL << FS_ENTRY_VALID_SHIFT;
			uint64_t Block = FSAllocBlock();
			if (Block == 0) return;
			FSFormatBlock(Block);
			Entries[i] |= Block << FS_ENTRY_MYBLOCK_SHIFT;
			FS_DriveWrite(1, CurHeader.DataLBA, Entries);
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
		FS_DriveWrite(1, CurHeader.DataLBA, Entries);
	}
}

static void FSRemoveAtCleanUpFile(uint64_t Block)
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

static void FSRemoveAtCleanUp(uint64_t Block)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);
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

	FS_DriveWrite(1, CurHeader.DataLBA, Entries);
	FreeVM(Entries);
}

static void FSRemoveAt(uint64_t Block, char* Name)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);
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
				FS_DriveWrite(1, CurHeader.DataLBA, Entries);
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

static void FSWriteFileAt(uint64_t Block, char* Name, void* Data, size_t Sectors)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	if (!CurHeader.Valid) return;
	if (CurHeader.IsDir) return;
	if (Sectors == 0) return;
	while (true)
	{
		FS_DriveWrite(1, CurHeader.DataLBA, Data);
		Sectors--;
		if (Sectors == 0) break;
		
		Data += 512;

		if (CurHeader.NextBlock > 0) Block = CurHeader.NextBlock;
		else
		{
			CurHeader.NextBlock = FSAllocBlock();
			FSBlockHeader Header;
			Header.Valid = true;
			Header.NextBlock = 0;
			Header.Name = 0;
			Header.IsDir = false;
			FSSetHeader(CurHeader.NextBlock, Header);
			FSSetHeader(Block, CurHeader);
			Block = CurHeader.NextBlock;
		}

		CurHeader = FSQueryHeader(Block);
		if (CurHeader.Name) FreeVM(CurHeader.Name);
	}
}


static size_t FSReadFileAt(uint64_t Block, void* Data, size_t Sectors)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	if (!CurHeader.Valid) return 0;
	if (CurHeader.IsDir) return 0;
	if (Sectors == 0) return 0;
	size_t ReadBytes = 0;
	int i = 0;
	while (true)
	{
		ReadBytes += 512;
		i++;
		FS_DriveRead(1, CurHeader.DataLBA, Data);
		Sectors--;
		if (Sectors == 0) break;
		
		Data += 512;

		if (CurHeader.NextBlock == 0) break;
		Block = CurHeader.NextBlock;

		CurHeader = FSQueryHeader(Block); 
	}
	//asm volatile ("cli\nhlt" :: "a"(0x1212), "b"(ReadBytes));
	return ReadBytes;
}

static size_t FSFileSizeAt(uint64_t Block)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (CurHeader.Name) FreeVM(CurHeader.Name);
	if (!CurHeader.Valid) return 0;
	if (CurHeader.IsDir) return 0;
	size_t OutBytes = 0;
	while (true)
	{
		OutBytes += 512;
		if (CurHeader.NextBlock == 0) break;
		Block = CurHeader.NextBlock;
		CurHeader = FSQueryHeader(Block);
	}
	return OutBytes;
}

static size_t FSDirectorySizeAt(uint64_t Block)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (!CurHeader.Valid) return 0;
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);
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

static void FSListFilesAt(uint64_t Block, FileListEntry* List)
{
	FSBlockHeader CurHeader = FSQueryHeader(Block);
	if (!CurHeader.Valid) return;
	uint64_t* Entries = AllocPhys(512);
	FS_DriveRead(1, CurHeader.DataLBA, Entries);
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

static bool FSMkdir(char* Dir)
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

static bool FSMkfile(char* File)
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

static bool FSRemove(char* Any)
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
static bool FSWriteFile(char* File, void* Data, size_t Bytes)
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
			uint64_t NextBlock = FSGetFileInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return false; // File doesnt exist
			FSWriteFileAt(NextBlock, TempBuf, Data, Bytes / 512 + 1);
			return true;
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}

static void* FSReadFile(char* File, size_t* BytesRead)
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
			uint64_t NextBlock = FSGetFileInDir(CurrentBlock, TempBuf);
			if (NextBlock == 0) return 0; // File doesnt exist
			size_t FileSize = FSFileSizeAt(NextBlock) / 512;
			void* Out = AllocPhys(FileSize * 512 * 4);
			*BytesRead = FSReadFileAt(NextBlock, Out, FileSize);
			return Out;
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}

static size_t FSFileSize(char* File)
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
			return FSFileSizeAt(NextBlock);
		}

		for (int i = 0;i < 256;i++)
		{
			TempBuf[i] = 0;
		}
		
		i++;
	}
}

static FileListEntry* FSListFiles(char* Dir, size_t* NumEntries)
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

static void CustomFS_Driver_Init(DriverMan_FilesystemDriver* MyDriver)
{
	// No initialization needed
}

static void CustomFS_Driver_Format(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device)
{
	FSDevice = Device;
	FSFormat();
}

static bool CustomFS_Driver_TryFormat(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device)
{
	FSDevice = Device;
	return FSTryFormat();
}

static bool CustomFS_Driver_IsFormatted(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device)
{
	FSDevice = Device;
	return FSIsFormatted();
}

static FileListEntry* CustomFS_Driver_ListFiles(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* Dir, size_t* NumFiles)
{
	FSDevice = Device;
	return FSListFiles(Dir, NumFiles);
}

static bool CustomFS_Driver_MakeFile(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* File)
{
	FSDevice = Device;
	return FSMkfile(File);
}

static bool CustomFS_Driver_MakeDir(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* Dir)
{
	FSDevice = Device;
	return FSMkdir(Dir);
}

static bool CustomFS_Driver_WriteFile(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* File, void* Data, size_t Bytes)
{
	FSDevice = Device;
	return FSWriteFile(File, Data, Bytes);
}

static void* CustomFS_Driver_ReadFile(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* File, size_t* BytesRead)
{
	FSDevice = Device;
	return FSReadFile(File, BytesRead);
}

static size_t CustomFS_Driver_FileSize(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* File)
{
	FSDevice = Device;
	return FSFileSize(File);
}

static bool CustomFS_Driver_Remove(DriverMan_FilesystemDriver* MyDriver, DriverMan_StorageDevice* Device, char* AnyDir)
{
	FSDevice = Device;
	return FSRemove(AnyDir);
}

DriverMan_FilesystemDriver* CustomFS_GetDriver()
{
	DriverMan_FilesystemDriver* OutDriver = AllocVM(sizeof(DriverMan_FilesystemDriver));
	
	OutDriver->Header.Name = "CustomFS";
	OutDriver->Header.Version = 0x00010000;
	OutDriver->DriverInit = CustomFS_Driver_Init;
	OutDriver->FilesysFormat = CustomFS_Driver_Format;
	OutDriver->FilesysTryFormat = CustomFS_Driver_TryFormat;
	OutDriver->FilesysListFiles = CustomFS_Driver_ListFiles;
	OutDriver->FilesysMakeFile = CustomFS_Driver_MakeFile;
	OutDriver->FilesysMakeDir = CustomFS_Driver_MakeDir;
	OutDriver->FilesysWriteFile = CustomFS_Driver_WriteFile;
	OutDriver->FilesysReadFile = CustomFS_Driver_ReadFile;
	OutDriver->FilesysRemove = CustomFS_Driver_Remove;
	OutDriver->FilesysIsFormatted = CustomFS_Driver_IsFormatted;
	OutDriver->FilesysFileSize = CustomFS_Driver_FileSize;

	return OutDriver;
}