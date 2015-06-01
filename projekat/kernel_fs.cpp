#include "kernel_fs.h"

KernelFS::KernelFS()
{
    
}

KernelFS::~KernelFS()
{
    
}

/*
montira particiju/disk
*/
char KernelFS::mount(Partition* partition)
{
    for (int i = 0; i < 26; i++)
    {
        if (false == disks[i].used)
        {
            disks[i].used = true;
            disks[i].disk = new Disk(partition);
            return i+65;
        }
    }
    return 0;
}

/*
demontira particiju/disk
*/
char KernelFS::unmount(char part)
{
    int idx = part-65;
    if (idx<0 || idx>25 || false == disks[idx].used)
        return 0;
    // TODO: blokiranje niti dok se ne zatvore svi fajlovi                      
    disks[idx].used = false;
    delete disks[idx].disk;
    disks[idx].disk = 0;
	return 1;
}

/*
formatira particiju/disk
*/
char KernelFS::format(char part)
{
    int idx = part-65;
    if (idx<0 || idx>25 || false==disks[idx].used)
	    return 0;

    // TODO: blokiranje niti dok se ne zatvore svi fajlovi

    Partition* p = disks[idx].disk->partition;
    // formatiranje particije na disku
    char res = ::format(*(disks[idx].disk));
    // re-ucitavanje podataka u memoriju
    delete disks[idx].disk;
    disks[idx].disk = new Disk(p);
    return res;
}

/*
proverava posotjanje fajla/foldera sa zadatom putanjom
*/
char KernelFS::doesExist(char* fname)
{
    PathParser ppath;
    parse(ppath, fname);
    int idx = ppath.disk - 65;
    if (idx<0 || idx>25 || false == disks[idx].used)
        return 0;

    Disk& d = *disks[idx].disk;

    Entry dir;
    dir.attributes = 0x03;
    dir.firstCluster = d.meta.rootDir;
    dir.size = d.meta.rootSize;

    bool fnd = false;
    for (uint8_t i = 0; i < ppath.partsNum; i++)
    {
        if (dir.size == 0 || (i==ppath.partsNum-1 && dir.attributes==0x01))
            return 0;
        Entry* entries = new Entry[dir.size];
        listDir(d, dir, entries);
        fnd = false;
        for (uint8_t eid = 0; eid < dir.size; eid++)
        {
            if (matchName(entries[eid], ppath.parts[i]))
            {
                dir = entries[eid];
                fnd = true;
                break;
            }
        }
        delete[] entries;
        if (!fnd) return 0;
    }

    return 1;
}

/*
-------------- TODO -------------- TODO --------------
otvara fajl ili pravi novi
*/
File* KernelFS::open(char* fname, char mode)
{

    return 0;
}

/*
-------------- TODO -------------- TODO --------------
brise fajl
*/
char KernelFS::deleteFile(char* fname)
{
    // obrisi ga iz foldera
    // nadovezi *free
	return 0;
}

/*
pravi folder
*/
char KernelFS::createDir(char* dirname)
{
    PathParser ppath;
    parse(ppath, dirname);
    int idx = ppath.disk - 65;
    if (idx<0 || idx>25 || false == disks[idx].used)
        return 0;

    Disk& d = *disks[idx].disk;
    Entry e;

    // nadji folder gde treba napraviti novi entry
    if (getEntry(d, e, combine(ppath, ppath.partsNum - 1)))
    {
        // napravi novi entry
        Entry newFolder;
        newFolder.attributes = 0x02;
        newFolder.firstCluster = allocate(d);
        d.FAT[newFolder.firstCluster] = 0;
        for (uint8_t i = 0; i < 8; newFolder.name[i++] = '\0');
        for (uint8_t i = 0; i < 3; newFolder.ext[i++] = '\0');
        memcpy(newFolder.name, ppath.parts[ppath.partsNum - 1], SOC*strlen(ppath.parts[ppath.partsNum - 1]));
        newFolder.size = 0;

        // ima mesta u trenutnom klasteru
        if (e.size == 0 || e.size % 102 != 0)
        {
            // nadji poslednji klaster
            ClusterNo cid = e.firstCluster;
            while (d.FAT[cid] != 0) cid = d.FAT[cid];
            // procitaj klaster
            char* w_buffer = new char[2048];
            Entry* entries = (Entry*)w_buffer;
            readCluster(d, cid, w_buffer);
            // mesto za smestanje entry-ja
            entries[(e.size + 102) % 102] = newFolder;
            writeCluster(d, cid, w_buffer);
            delete[] w_buffer;
            // apdejtuj velicinu nadfoldera
            if (e.attributes == 0x03)
            {
                d.meta.rootSize++;
            }
            else
            {
                // nadji nad folder, za promeni velicine
                Entry _dir;
                getEntry(d, _dir, combine(ppath, ppath.partsNum - 2));
                char w_buffer[2048];
                Entry* e_buffer = (Entry*)w_buffer;

                ClusterNo brojUlaza = _dir.size;

                ClusterNo brojKlastera = (_dir.size + 101) / 102;
                ClusterNo preostalo = brojUlaza;
                ClusterNo cid = _dir.firstCluster;
                for (uint8_t i = 0; i < brojKlastera; i++)
                {
                    readCluster(d, cid, w_buffer);
                    e_buffer = (Entry*)w_buffer;
                    uint8_t limit = preostalo < 102 ? preostalo : 102;
                    bool fnd = false;
                    for (uint8_t eid = 0; eid < limit; eid++)
                    {
                        if (e_buffer[eid].firstCluster == e.firstCluster)
                        {
                            fnd = true;
                            e_buffer[eid].size++;
                            writeCluster(d, cid, w_buffer);
                            break;
                        }
                    }
                    if (fnd) break;
                    cid = d.FAT[cid];
                }
            }
        }
        // nema mesta u trenutnom klasteru TODO ...
        else
        {
            // TODO TODO
        }
        return 1;
    }
    return 0;
}

/*
-------------- TODO -------------- TODO --------------
brise folder
*/
char KernelFS::deleteDir(char* dirname)
{
    // obrisi ga iz parent foldera
    // nadovezi *free

    PathParser ppath;
    parse(ppath, dirname);
    int idx = ppath.disk - 65;
    if (idx<0 || idx>25 || false == disks[idx].used)
        return 0;

    Disk& d = *disks[idx].disk;
    Entry e;

    // nadji parent folder
    if (getEntry(d, e, combine(ppath, ppath.partsNum - 1)))
    {
        Entry* entries = new Entry[e.size];
        listDir(d, e, entries);
        Entry folder;

        for (uint8_t i = 0; i < e.size; i++)
        {
            if (matchName(entries[i], ppath.parts[ppath.partsNum - 1]))
            {
                // nasao entry
                Entry folder = entries[i];

                // oslobodi klastere koji pripadaju folderu TODO

                // ukloni iz naddirektorijuma TODO
                ClusterNo totalClusters;
                if (e.size - i < 102)
                {
                }

                break;
            }
        }
        delete[] entries;
    }
	return 0;
}

/*
otvara n-ti Entry unutar foldera
*/
char KernelFS::readDir(char* dirname, EntryNum n, Entry &e)
{
    PathParser ppath;
    parse(ppath, dirname);
    int idx = ppath.disk - 65;
    if (idx<0 || idx>25 || false == disks[idx].used)
        return 0;

    Disk& d = *disks[idx].disk;
    Entry dir;

    // nadji folder gde treba napraviti novi entry
    if (getEntry(d, dir, dirname))
    {
        Entry* entries = new Entry[dir.size];
        listDir(d, dir, entries);
        if (n >= 0 && n < dir.size)
        {
            e = entries[n];
            delete[] entries;
            return 1;
        }
    }
	return 0;
}