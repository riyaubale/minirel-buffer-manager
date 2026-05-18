#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include "page.h"
#include "buf.h"

#define ASSERT(c)                                              \
    {                                                          \
        if (!(c))                                              \
        {                                                      \
            cerr << "At line " << __LINE__ << ":" << endl      \
                 << "  ";                                      \
            cerr << "This condition should hold: " #c << endl; \
            exit(1);                                           \
        }                                                      \
    }

//----------------------------------------
// Constructor of the class BufMgr
//----------------------------------------

BufMgr::BufMgr(const int bufs)
{
    numBufs = bufs;

    bufTable = new BufDesc[bufs];
    memset(bufTable, 0, bufs * sizeof(BufDesc));
    for (int i = 0; i < bufs; i++)
    {
        bufTable[i].frameNo = i;
        bufTable[i].valid = false;
    }

    bufPool = new Page[bufs];
    memset(bufPool, 0, bufs * sizeof(Page));

    int htsize = ((((int)(bufs * 1.2)) * 2) / 2) + 1;
    hashTable = new BufHashTbl(htsize); // allocate the buffer hash table

    clockHand = bufs - 1;
}

BufMgr::~BufMgr()
{

    // flush out all unwritten pages
    for (int i = 0; i < numBufs; i++)
    {
        BufDesc *tmpbuf = &bufTable[i];
        if (tmpbuf->valid == true && tmpbuf->dirty == true)
        {

#ifdef DEBUGBUF
            cout << "flushing page " << tmpbuf->pageNo
                 << " from frame " << i << endl;
#endif

            tmpbuf->file->writePage(tmpbuf->pageNo, &(bufPool[i]));
        }
    }

    delete hashTable;
    delete[] bufTable;
    delete[] bufPool;
}

const Status BufMgr::allocBuf(int &frame)
{
    int count = 0;
    while (count < 2 * (int)numBufs)
    {
        advanceClock();
        count++;

        if (!bufTable[clockHand].valid)
        {
            frame = clockHand;
            return OK;
        }

        if (bufTable[clockHand].refbit)
        {
            bufTable[clockHand].refbit = false;
            continue;
        }

        if (bufTable[clockHand].pinCnt == 0)
        {
            if (bufTable[clockHand].dirty)
            {
                Status status = bufTable[clockHand].file->writePage(bufTable[clockHand].pageNo, &(bufPool[clockHand])); // write page back to disk
                if (status != OK)
                    return UNIXERR;
                bufStats.diskwrites++;
            }

            hashTable->remove(bufTable[clockHand].file, bufTable[clockHand].pageNo); // remove older entry from hash table
            bufTable[clockHand].Clear();
            frame = clockHand;
            return OK;
        }
    }
    return BUFFEREXCEEDED; // all buffer frames are pinned
}

const Status BufMgr::readPage(File *file, const int PageNo, Page *&page)
{
    int frame;
    Status status;
    status = hashTable->lookup(file, PageNo, frame);

    // case 1: page is not in buffer pool
    if (status == HASHNOTFOUND)
    {
        status = allocBuf(frame); // allocate a buffer frame
        if (status == BUFFEREXCEEDED)
            return BUFFEREXCEEDED;
        else if (status == UNIXERR)
            return UNIXERR;

        status = file->readPage(PageNo, &bufPool[frame]); // read page from disk into buffer pool frame
        if (status == UNIXERR)
            return UNIXERR;

        status = hashTable->insert(file, PageNo, frame); // insert page into hashtable
        if (status == HASHTBLERROR)
            return HASHTBLERROR;

        bufTable[frame].Set(file, PageNo); // set up frame
        page = &bufPool[frame];
        bufStats.diskreads++;
        bufStats.accesses++;
    }
    // case 2: page is in buffer pool
    else
    {
        bufTable[frame].refbit = true;
        bufTable[frame].pinCnt++;
        page = &bufPool[frame];
        bufStats.accesses++;
    }
    return OK;
}

const Status BufMgr::unPinPage(File *file, const int PageNo,
                               const bool dirty)
{
    int frame;
    Status status;
    status = hashTable->lookup(file, PageNo, frame);

    if (status == HASHNOTFOUND)
        return HASHNOTFOUND;

    if (bufTable[frame].pinCnt == 0)
        return PAGENOTPINNED;

    bufTable[frame].pinCnt--;
    if (dirty)
        bufTable[frame].dirty = true;
    return OK;
}

const Status BufMgr::allocPage(File *file, int &pageNo, Page *&page)
{
    int frame;
    Status status;

    status = file->allocatePage(pageNo); // allocate empty page
    if (status != OK)
        return UNIXERR;

    status = allocBuf(frame);
    if (status == BUFFEREXCEEDED)
        return BUFFEREXCEEDED;
    else if (status == UNIXERR)
        return UNIXERR;

    status = hashTable->insert(file, pageNo, frame); // entry is inserted into hash table
    if (status == HASHTBLERROR)
        return HASHTBLERROR;

    bufTable[frame].Set(file, pageNo);
    page = &bufPool[frame];
    return OK;
}

const Status BufMgr::disposePage(File *file, const int pageNo)
{
    // see if it is in the buffer pool
    Status status = OK;
    int frameNo = 0;
    status = hashTable->lookup(file, pageNo, frameNo);
    if (status == OK)
    {
        // clear the page
        bufTable[frameNo].Clear();
    }
    status = hashTable->remove(file, pageNo);

    // deallocate it in the file
    return file->disposePage(pageNo);
}

const Status BufMgr::flushFile(const File *file)
{
    Status status;

    for (int i = 0; i < numBufs; i++)
    {
        BufDesc *tmpbuf = &(bufTable[i]);
        if (tmpbuf->valid == true && tmpbuf->file == file)
        {

            if (tmpbuf->pinCnt > 0)
                return PAGEPINNED;

            if (tmpbuf->dirty == true)
            {
#ifdef DEBUGBUF
                cout << "flushing page " << tmpbuf->pageNo
                     << " from frame " << i << endl;
#endif
                if ((status = tmpbuf->file->writePage(tmpbuf->pageNo,
                                                      &(bufPool[i]))) != OK)
                    return status;

                tmpbuf->dirty = false;
            }

            hashTable->remove(file, tmpbuf->pageNo);

            tmpbuf->file = NULL;
            tmpbuf->pageNo = -1;
            tmpbuf->valid = false;
        }

        else if (tmpbuf->valid == false && tmpbuf->file == file)
            return BADBUFFER;
    }

    return OK;
}

void BufMgr::printSelf(void)
{
    BufDesc *tmpbuf;

    cout << endl
         << "Print buffer...\n";
    for (int i = 0; i < numBufs; i++)
    {
        tmpbuf = &(bufTable[i]);
        cout << i << "\t" << (char *)(&bufPool[i])
             << "\tpinCnt: " << tmpbuf->pinCnt;

        if (tmpbuf->valid == true)
            cout << "\tvalid\n";
        cout << endl;
    };
}
