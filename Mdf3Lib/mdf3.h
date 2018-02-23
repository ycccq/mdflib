#pragma once

#include <map>
#include "mdfConfig.h"
#include "dynArray.h"
#include "mdFile.h"
#include "utf8.h"
#include <assert.h>

#define MDF3LIB_VERSION L"1.001 2018/02/21"
extern M_UNICODE MDF3LibGetVersionString();

//Define Each Block ID
#define M3ID_ID MDF_ID('#','#')  // common ID prefix
#define M3ID_HD MDF_ID('H','D')  // Gerneral Description of the measurement file
#define M3ID_TX MDF_ID('T','X')  // Contains a string with a variable length
#define M3ID_PR MDF_ID('P','R')  // Contains property data of the application generating the MDF file
#define M3ID_DG MDF_ID('D','G')  // Description of data block that may refer to one or serveral channel groups
#define M3ID_CG MDF_ID('C','G')  // Description of channel group, i.e. signals which are always measured jointly
#define M3ID_CN MDF_ID('C','N')  // Description of a channel
#define M3ID_CC MDF_ID('C','C')  // Description of a conversion formula for a channel
#define M3ID_TR MDF_ID('T','R')  // Description of a trigger event
#define M3ID_CD MDF_ID('C','D')  // Description of dependency between channels
#define M3ID_CE MDF_ID('C','E')  // Additional information about the data source of the channel
#define M3ID_SR MDF_ID('S','R')  // Description of a sample reduction, i.e. alternative data rows for a channel group with usually lower number of samples


//-------------------------------------------------------------------------------------------------------
// MDF3 - common Header
//-------------------------------------------------------------------------------------------------------
struct m3BlockHdr
{
	M_UINT16 hdr_hdr;   // '##'
	M_UINT16 hdr_id;    // 'XX'
	M_UINT64 hdr_len;   // Length of block in bytes
	M_UINT64 hdr_links; // # of links 
};


// ##HD Header Block
//-------------------------------------------------------------------------------------------------------
// The HDBLOCK always begins at file position 64. It contains general information about the contents
// of the measured data file and is the root for the block hierarchy
//-------------------------------------------------------------------------------------------------------

struct M3HDRecord {
	// Block Header
	enum { RID = M3ID_HD };
	// Block size
	M_INT16 hd_block_size;
	// enumeration of links
	enum {
		hd_dg_first, //Pointer to the first data group block(DGBLOCK), NIL allowed
		hd_tx, //Pointer to the measurement file comment text(TXBLOCK), NIL allowed
		hd_pr, //Pointer to prorgram block(PRBLOCK), NIL allowed
		Linkmax // # of know links
	};
	//Data memebers
	M_CHAR hd_date[10];
	M_CHAR hd_time[8];
	M_CHAR hd_author[32];
	M_CHAR hd_organization[32];
	M_CHAR hd_project[32];
	M_CHAR hd_subject[32];
	M_UINT64 hd_timestamp;
	M_INT16 hd_utc_time_offset;
	M_UINT16 hd_time_quality_class;
	M_CHAR hd_time_identification;
};


//---pure virtual base class for all blocks ------------------
// overwrite
//   getSize() - return the size of the block (w/o hdr/link)
//   getData() - return @ of payload[i] - return NULL if done
//   hdrId()   - return ID of block (e.g. 'HD')

class m3Block {
public:
	m3Block(size_t initLinks = 0) :m_Links(initLinks), m_File(NULL), m_At(0) {
		memset(&m_Hdr, 0, sizeof(m_Hdr));
		m_Hdr.hdr_hdr = M3ID_ID;
	}
	virtual ~m3Block() {}

	// Record Layout
	// common Header
	// + nLinks*sizeof(M_LINK)
	// + FixedSize
	// + VariablePart(s)
	virtual M_UINT32 getFixedSize() { return 0; }
	virtual void *   getFixedPart() { return NULL; }

	//-------------------------------------------------
	// Output blocks
	//-------------------------------------------------
	// getSize() & getData(): without common header
	virtual M3SIZE getSize() = 0;
	virtual PVOID  getData(int Index, M3SIZE &szRemain) = 0;

	virtual BOOL Read(mDirectFile *File, M3LINK At, const m3BlockHdr &h);
	virtual BOOL readData(M3LINK At, M3SIZE szRemain) = 0;

	//-------------------------------------------------
	// LINK section
	//-------------------------------------------------
	bool hasLink(size_t linkNo)
	{
		return linkNo < m_Links.getSize() &&
			*m_Links.get(linkNo) != 0;
	}
	void setLink(size_t linkNo, M3LINK at)
	{
		*m_Links.get(linkNo) = at;
	}
	M3LINK getLink(size_t linkNo)
	{
		if (linkNo >= m_Links.getSize())
			return 0;
		else
			return *m_Links.get(linkNo);
	}

	// return file position
	M3LINK Link() const { return m_At; }
	// get the RID
	virtual M_UINT16 hdrID() const
	{
		return m_Hdr.hdr_id;
	}

public:
	mDirectFile * m_File; //associated file
	M3LINK m_At; //position
	m3BlockHdr m_Hdr; //the common header
	dynArray<M3LINK> m_Links;//dynamic Array with Links
};

//-------------------------------------------------------------------------
// template to associate data structures with m3Blocks
// template Arguments
//   class R = RecordType
//   class T = variable part
//   ID=M3_ID
// Note: the object is derived from m3Block as well as from R
//-------------------------------------------------------------------------
template<class R, class T = M_BYTE, int ID = R :RID>class m3BlockImpl :public m3Block, public R {
public:
	m3BlockImpl(size_t nVar = 0) :m3Block(R::LinkMax), m_var(nVar) {
		m_Hdr.hdr_hdr = M3ID_ID;
		m_Hdr.hdr_id = ID;
		R *pThis = static_cast<R*>(this);// cast this pointer to RecordType pointer, why?
		memset(pThis, 0, sizeof(R));
	}
	virtual ~m3BlockImpl() {}
	virtual M_UINT32 getFixedSize() { return sizeof(R); }
	virtual void* getFixedPart() { return static_cast<R*>(this); }

	//
	virtual BOOL readData(M3LINK At, M3SIZE szRemain)
	{
		// A BLOCK consists of fixed part and variable part

		// read the fixed Part
		R *pThis = static_cast<R *>(this);
		M3SIZE szThis = sizeof(R);
		if (szThis > szRemain) szThis = szRemain;
		if (!m_File->ReadAt(At, (M_UINT32)szThis, pThis))
			return FALSE;
		if ((szRemain -= szThis) > 0)
		{
			// read the variable part
			PVOID pVar = m_var.resize((size_t)szRemain);
			At += szThis;
			if (!m_File->ReadAt(At, (M_UINT32)szRemain, pVar))
				return FALSE;
		}
		return TRUE;
	}

	virtual M3SIZE getSize() { return m_var.getBytes(); }
	virtual PVOID  getData(int Index, M3SIZE &szRemain)
	{
		if (Index == 0)
		{
			assert(szRemain == m_var.getBytes());
			return szRemain ? m_var.get() : NULL;
		}
		else
		{
			assert(FALSE);
		}
		return NULL;
	}

	//The following two function used in TX and MD BLOCK
	//setCommentBLK
	//setText

	T *get(size_t Index = 0) { return m_var.get(Index); }

public:
	dynArray<T> m_var;// represents data content in BYTE in MDF file
};

// helper struct to count stored records for ChannelGroups (RecordId)
// also used to count messages per bus (Id,count)
struct idCount
{
	M_UINT64 id;  // (DGNum<<45)|recId
	M_LINK aov;
	M_SIZE cnt;
};
typedef std::map<M_UINT64, idCount> idCounts;

class M3DGBlock;
class M3TXBlock;
class M3HDBlock;
class M3PRBlock;

//MDF3File class, used to manipulate file
class MDF3File :public mDirectFile {
public:
	MDF3File();
	virtual ~MDF3File();
	virtual void Close();

	BOOL Create(M_FILENAME strPathName, const char *strProducer = NULL, int iVersion = 330);
	BOOL Open(M_FILENAME strPathName, BOOL bUpdate = FALSE);
	m3Block *LoadBlock(M3LINK At);
	//m3Block *LoadLink(m3Block &parent,int linkNo);
	m3Block *LoadLink(m3Block &parent, int linkNo, M_UINT16 id = 0);
	bool     LoadBlkHdr(M3LINK At, m3BlockHdr &h);

protected:
	mdfFileId m_Id;
	M3HDBlock m_Hdr;
	idCounts m_recCnt;
};

class M3HDBlock :public m3BlockImpl<M3HDRecord> {
public:
	// data member in M3HDBlock
	M3DGBlock *m_dgNext;
	M3TXBlock *m_txNext;
	M3PRBlock *m_prNext;

protected:
	bool m_bPrepared;// flag used for save() when close() is called

public:
	M3HDBlock() {
		m_dgNext = NULL;
		m_txNext = NULL;
		m_prNext = NULL;
		m_bPrepared = false;
	}
	virtual ~M3HDBlock() {
		delete m_dgNext;
		delete m_txNext;
		delete m_prNext;
	}

	BOOL load(mDirectFile *f);

};