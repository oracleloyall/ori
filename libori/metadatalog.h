#ifndef __METADATALOG_H__
#define __METADATALOG_H__

#include <map>
#include <string>
#include <tr1/memory>

#include "objecthash.h"

typedef uint32_t refcount_t;
typedef std::map<ObjectHash, refcount_t> RefcountMap;

class MetadataLog;
class MdTransaction
{
public:
    MdTransaction(MetadataLog *log);
    ~MdTransaction();
    typedef std::tr1::shared_ptr<MdTransaction> sp;
private:
    friend class MetadataLog;
    MetadataLog *log;
    RefcountMap counts;
};

class MetadataLog
{
public:
    MetadataLog();
    ~MetadataLog();

    bool open(const std::string &filename);
    /// rewrites the log file, optionally with new counts
    void rewrite(const RefcountMap *refs = NULL);

    void addRef(const ObjectHash &hash, MdTransaction::sp trs =
            MdTransaction::sp());
    refcount_t getRefCount(const ObjectHash &hash) const;

    MdTransaction::sp begin();
    void commit(MdTransaction *tr);

private:
    int fd;
    RefcountMap refcounts;
};

#endif
