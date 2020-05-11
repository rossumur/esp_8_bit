
/* Copyright (c) 2020, Peter Barrett
**
** Permission to use, copy, modify, and/or distribute this software for
** any purpose with or without fee is hereby granted, provided that the
** above copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
** BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
** OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
** WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
** ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
** SOFTWARE.
*/

#include "emu.h"
using namespace std;

// Map files into memory for carts bigger than physical RAM
// Handly for NES/SMS carts
// Uses app1 as a cache with a crappy FS on top - default arduino config gives 1280k

#ifdef ESP_PLATFORM
#include <esp_spi_flash.h>
#include <esp_attr.h>
#include <esp_partition.h>
#include "rom/miniz.h"

// only map 1 file at a time
spi_flash_mmap_handle_t _file_handle = 0;

static void print_part(const esp_partition_t *pPart)
{
    printf("main: partition type = %d.\n", pPart->type);
    printf("main: partition subtype = %d.\n", pPart->subtype);
    printf("main: partition starting address = %x.\n", pPart->address);
    printf("main: partition size = %x.\n", pPart->size);
    printf("main: partition label = %s.\n", pPart->label);
    printf("main: partition encrypted = %d.\n", pPart->encrypted);
    printf("\n");
}

static void print_parts(esp_partition_iterator_t it)
{
    while (it)
    {
        print_part((esp_partition_t *) esp_partition_get(it));
        it = esp_partition_next(it);
    }
}

typedef struct {
    uint32_t sig;
    uint32_t offset;
    uint32_t len;
    uint32_t flags;
    char name[128-16];
} FlashFile;

#define FSIG ('F' | ('I' << 8) | ('L' << 16) | ('E' << 24))

class CrapFS {
public:
    #define DIR_BLOCK_SIZE 0x4000   // 16k or 128 entries
    // skip 64K at start of partition

    const esp_partition_t* _part;
    uint8_t *_buf;
    FlashFile* _dir;
    int _count;

    CrapFS() : _buf(0),_count(0)
    {
        _part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, "app1");
        if (!_part) {
            printf("CrapFS::CrapFS app1 not found\n");
            return;
        }
        print_part(_part);

        _buf = new uint8_t[DIR_BLOCK_SIZE];
        if (!_buf)
            return;
        _dir = (FlashFile*)_buf;
        esp_err_t err = esp_partition_read(_part, 0, _buf, DIR_BLOCK_SIZE);
        if (err) {
            printf("CrapFS::CrapFS dir read failed %d\n",err);
            return;
        }

        // init directory if required
        _count = DIR_BLOCK_SIZE/sizeof(FlashFile);
        if (_dir[0].sig != FSIG)
            reformat();

        // dump dir
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig == FSIG)
                printf("%08X %08X %s\n",_dir[i].offset,_dir[i].len,_dir[i].name);
        }
    }

    ~CrapFS()
    {
        delete _buf;
    }

    uint32_t align(uint32_t n)
    {
        return (n + 0xFFFF) & 0xFFFF0000;
    }

    void reformat()
    {
        esp_partition_erase_range(_part,0,_part->size); // erase entire partition
        memset(_buf,0xFF,sizeof(DIR_BLOCK_SIZE));       // only use the first 16k of first block
    }

    uint8_t* mmap(const FlashFile* file)
    {
        if (!file)
            return 0;
        printf("CrapFS::mmap mapping %s offset:%08X len:%d\n",file->name,file->offset,file->len);
        void* data = 0;
        if (esp_partition_mmap(_part, file->offset, file->len, SPI_FLASH_MMAP_DATA, (const void**)&data, &_file_handle) == 0)
        {
            printf("CrapFS::mmap mapped to %08X\n",data);
            return (uint8_t*)data;
        }
        return 0;
    }

    // see if the file exists
    FlashFile* find(const std::string& path)
    {
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig == FSIG && strcmp(_dir[i].name,path.c_str()) == 0)
                return _dir + i;
        }
        return NULL;
    }

    int copy(const std::string& path, int offset, int len)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f)
            return -1;
        #define BUF_SIZE 4096
        uint8_t* buf = new uint8_t[BUF_SIZE];
        esp_err_t err;
        int i = 0;
        while (i < len) {
            int n = len-i;
            if (n > BUF_SIZE)
                n = BUF_SIZE;
            fread(buf,1,n,f);
            printf("CrapFS::copy writing %d of %d\n",i,len);
            err = esp_partition_write(_part, i + offset, buf, n);
            if (err)
                break;
            i += n;
        }
        fclose(f);
        delete buf;
        return err;
    }

    //
    FlashFile* create(const std::string& path, int len)
    {
        uint32_t start = 0x10000;
        for (int i = 0; i < _count; i++) {
            if (_dir[i].sig != FSIG) {
                _dir[i].sig = FSIG;
                _dir[i].offset = start;
                _dir[i].len = len;  //
                strcpy(_dir[i].name,path.c_str());
                printf("CrapFS::created %s %08X %d\n",_dir[i].name,start,len);
                esp_err_t err = esp_partition_erase_range(_part,0, DIR_BLOCK_SIZE);     // erase dir
                if (err == 0)
                    err = esp_partition_write(_part, 0, _buf, DIR_BLOCK_SIZE);    // update dir
                if (err) {
                    printf("CrapFS::create dir write failed %d\n",err);
                    return NULL;
                }
                if (copy(path,start,len))
                    return NULL;
                return _dir+i;
            }
            start = align(_dir[i].offset + _dir[i].len);
        }
        // create failed. might want to invoke nuclear option
        return NULL;
    }
};

uint8_t* map_file(const char* path, int len)
{
    CrapFS _fs;
    FlashFile* file = _fs.find(path);   // already copied?
    if (!file)
        file = _fs.create(path,len);    // need to create a new file
    if (!file) {
        _fs.reformat();
        file = _fs.create(path,len);    // need to create a new file after reformatting the cache
    }
    return _fs.mmap(file);
}

void unmap_file(uint8_t* ptr)
{
    if (_file_handle)
        spi_flash_munmap(_file_handle);
    _file_handle = 0;
}

FILE* mkfile(const char* path)
{
    return fopen(path,"wb");
}

#else
#include <sys/stat.h>
#include "../miniz.h"

uint8_t* map_file(const char* path, int len)
{
    uint8_t* d;
    Emu::load(path,&d,&len);
    return d;
}

void unmap_file(uint8_t* ptr)
{
    delete ptr;
}

FILE* mkfile(const char* path)
{
    std::string v = path;
    std::string dir = v.substr(0,v.find_last_of("/"));
    mkdir(dir.c_str(), 0755);
    return fopen(path,"wb");
}

#endif

// map one bit array to another
uint32_t generic_map(uint32_t bits, const uint32_t* m)
{
    uint32_t b = 0;
    for (int i = 0; i < 16; i++) {
        if ((0x8000 >> i) & bits)
            b |= m[i];
    }
    return b;
}

// unpack file and write to FS, use rom miniz on esp32
// uses quite a lot of memory, call before initializing screen on atari
// could actually use the screen mem (which might look cool) or the main cpu mem for buffer
int unpack(const char* dst, const uint8_t* d, int len)
{
    printf("unpacking %s\n",dst);
    FILE* f = mkfile(dst);
    if (!f)
        return -1;

    #define BUF_SIZE 0x8000
    uint8_t* buf = new uint8_t[BUF_SIZE];
    if (!buf) {
        fclose(f);
        return -1;  // could use a smaller window on compression but would not generalize to other people's zips
    }

    tinfl_decompressor* dec = new tinfl_decompressor;   // largist
    size_t in_bytes, out_bytes;
    tinfl_status status;
    int i = 0;

    tinfl_init(dec);
    while (i < len) {
        in_bytes = len-i;
        out_bytes = BUF_SIZE;
        status = tinfl_decompress(dec,d+i,&in_bytes,buf,buf,&out_bytes,11);
        if (out_bytes != fwrite(buf,1,out_bytes,f)) {
            status = TINFL_STATUS_FAILED;
            break;
        }
        i += in_bytes;
    }

    delete [] buf;
    delete dec;
    fclose(f);

    if (status == TINFL_STATUS_FAILED) {
        remove(dst);
        return -1;
    }
    return 0;
}

Emu::Emu(const char* n,int w,int h, int st, int aformat, int cc, int f) :
    name(n),width(w),height(h),standard(st),audio_format(aformat),cc_width(cc),flavor(f)
{
    //audio_frequency = 15625; // requires fixed point sampler
    audio_frequency = standard == 1 ? 15720 : 15600;
    audio_frame_samples = standard ? (audio_frequency << 16)/60 : (audio_frequency << 16)/50;   // fixed point sampler
    audio_fraction = 0;
}

Emu::~Emu()
{
}

int Emu::frame_sample_count()
{
    int n = audio_frame_samples + audio_fraction;
    audio_fraction = n & 0xFFFF;
    return n >> 16;
}

int Emu::insert(const std::string& path, int flags, int disk_index)
{
    return -1;
}

const uint32_t* Emu::composite_palette()
{
    return standard ? ntsc_palette() : pal_palette();
}

// determine file type
int Emu::head(const std::string& path, uint8_t* data, int len)
{
    FILE *f = fopen(path.c_str() , "rb");
    if (!f)
        return -1;
    fread(data,1,len,f);
    fseek(f, 0, SEEK_END);
    int flen =(int)ftell(f);
    fclose(f);
    return flen;
}

int Emu::load(const std::string& path, uint8_t** data, int* len)
{
    *data = 0;
    *len = 0;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) {
        printf("Emu::load failed for %s\n",path.c_str());
        return -1;
    }
    fseek(f, 0, SEEK_END);
    int fsize = (int)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* d = new uint8_t[fsize];
    if (!d) {
        printf("Emu::load failed for %s (out of memory)\n",path.c_str());
        fclose(f);
        return -1;
    }
    fread(d, 1, fsize, f);
    fclose(f);

    printf("Emu::load %d bytes %s\n",fsize,path.c_str());
    *data = d;
    *len = fsize;
    return 0;
}
