#pragma once

class cData {
public:
	cData(){
		Reader=NULL;
		Writer=NULL;
	}
	~cData();
	void Clear();
	
	const cStr & GetFilePn() const { return m_FilePn; }
	void SetFilePn(const char *FilePn, bool ForWrite);
	
	bool IsEof();
	size_t Size();
	int GetPos();
	
	// Moves the data position pointer to a new location that is "Offset" bytes from origin.
	// Returns the offset, in bytes, of the new position from the beginning of the data.
	int SeekCur(const int Offset);	
	int SetPos(const int Pos);
	
	// Returns the number of bytes actually read, which may be less than MaxSize if the end of data
	// is encountered before reaching MaxSize.
	int Read(void *To, const int MaxSize);
	void Write(const void *Fm, const int Size);	
	
	byte ReadByte();
	bool ReadByte(byte *b) ;
	void WriteByte(const byte b);
	
	short ReadShort();
	bool ReadShort(short *s) ;
	void WriteShort(const short s);

	word ReadWord();
	bool ReadWord(word *w);
	void WriteWord(const word w);

	int  ReadInt();
	bool ReadInt(int *i);
	void WriteInt(const int i);

	dword ReadDword();
	bool ReadDword(dword *dw);
	void WriteDword(const dword dw);

	float ReadFloat();
	bool ReadFloat(float *f);
	void WriteFloat(float f);

	double ReadDouble();
	bool ReadDouble(double *d);
	void WriteDouble(double d);
	
	bool ReadString(cStr *, const char *Terminators = "\r\n",int maxlen=-1);
	void WriteString(const char *);
private:
	cStr m_FilePn;
	FILE_ReadBinStream* Reader;	
	FILE_WriteBinStream* Writer;	
};
//*****************************************************************************
// cMeshCodec
//*****************************************************************************
class cMeshCodec {
public:
	cMeshCodec() {}
	virtual ~cMeshCodec() {}
	virtual cMeshContainer * Decode(cData &Fm) = 0;
	virtual void Encode(cMeshContainer &Mesh, cData *To) = 0;
	virtual bool CanEncode(){return true;}
	virtual bool CanDecode(){return true;}
};

//*****************************************************************************
// cIO
//*****************************************************************************
class cMeshIO {
public:
	static bool m_SwapYZ;
	static void Init();

	static bool AddCodec(const char *FileExtension, cMeshCodec *Codec);
	static cMeshContainer * LoadMesh(const char *FileName);
	static bool SaveMesh(cMeshContainer &Mesh, const char *FileName);	
	static void SwapYZ(cMeshContainer* M,bool Export);

	struct MeshCodecInfo {
		cStr FileExtension;
		cMeshCodec *Codec;
	};
	static cList<MeshCodecInfo> & GetMeshCodecs() { return m_MeshCodecs; }
private:	
	static cList<MeshCodecInfo> m_MeshCodecs;	
};
