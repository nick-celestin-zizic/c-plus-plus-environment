#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <math.h>

#ifndef NCZ_HPP_
#define NCZ_HPP_

// Basic Types
typedef const char* cstr;
typedef float       f32;
typedef double      f64;
typedef int8_t      s8;
typedef uint8_t     u8;
typedef int16_t     s16;
typedef uint16_t    u16;
typedef int32_t     s32;
typedef uint32_t    u32;
typedef int64_t     s64;
typedef uint64_t    u64;
typedef uintptr_t   usize;


#define assert(cond) if (!(cond)) ::ncz::failed_assert(#cond, ::ncz::Source_Location {__FILE__, __LINE__})

#pragma region ModuleParameters
#ifndef NCZ_PAGE_SIZE
#define NCZ_PAGE_SIZE 4096
#endif
#ifndef NCZ_DEFAULT_ALIGNMENT
#define NCZ_DEFAULT_ALIGNMENT 16
#endif
#pragma endregion

#pragma region FunkyMacros
#define NCZ_TEMP ::ncz::Allocator { ::ncz::pool_allocator_proc, &::ncz::context.temporary_storage }
#define NCZ_CONCAT_1(x, y) x##y
#define NCZ_CONCAT(x, y)  NCZ_CONCAT_1(x, y)
#define NCZ_GENSYM(x)     NCZ_CONCAT(x, __COUNTER__)
#define NCZ_DEFER(code) ::ncz::Defer NCZ_GENSYM(_defer_) {[&](){code;}}
#define NCZ_HERE ::ncz::Source_Location {__FILE__, __LINE__}

#define trace(...) log(::ncz::Source_Location {__FILE__, __LINE__}, ": ", __VA_ARGS__)
#define trace_error(...)  log_ex(Log_Level::NORMAL,  Log_Type::ERRO, ::ncz::Source_Location {__FILE__, __LINE__}, ": ", __VA_ARGS__)

#ifndef __clang__
#error "TODO: rant about c compilers"
#endif//__clang__

#ifndef NCZ_CFLAGS
#define NCZ_CFLAGS "-std=c++17", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-fsanitize=address", "-g", "-nostdinc++", "-fno-rtti", "-fno-exceptions"
#endif//NCZ_CFLAGS

#ifndef NCZ_RCFLAGS
#define NCZ_RCFLAGS "-std=c++17", "-nostdinc++", "-fno-rtti", "-fno-exceptions"
#endif//NCZ_RCFLAGS

#ifndef NCZ_LDFLAGS
#ifdef _WIN32
// we need this on windows to get nice stack traces
#define NCZ_LDFLAGS "-ldbghelp",   \
    "-Xlinker", "/INCREMENTAL:NO", \
    "-Xlinker", "/NOLOGO",         \
    "-Xlinker", "/NOIMPLIB",       \
    "-Xlinker", "/NODEFAULTLIB:msvcrt.lib"
#else
#define NCZ_LDFLAGS ""
#endif//_WIN32
#endif//NCZ_LDFLAGS

#ifndef NCZ_CC
#define NCZ_CC(binary_path, source_path) "clang", NCZ_CFLAGS, NCZ_LDFLAGS, "-o", binary_path, source_path
#else
#endif//NCZ_CC


// stolen nob Go Rebuild Urself™ Technology
// from: https://github.com/tsoding/musializer/blob/master/src/nob.h#L260
#define NCZ_CPP_FILE_IS_SCRIPT(argc, argv)  do {                             \
    context.logger.labels.push("config");                                    \
    maybe_reload_cpp_script({(argv), static_cast<usize>((argc))}, __FILE__); \
    context.logger.labels.pop();                                             \
} while (0)

#pragma endregion

namespace ncz {
    void log_stack_trace(int skip = 0);
    template <typename F>
    struct Defer {
        F f;
        Defer(F f) : f(f) {}
        ~Defer() { f(); }
    };
    struct Source_Location { cstr file; s64 line; };
    
    void failed_assert(cstr repr, Source_Location loc);
    
#pragma region MemoryPrimitives
    // Allocator Type
    enum class Allocator_Mode {
        ALLOCATE = 0,
        RESIZE   = 1,
        DISPOSE  = 2,
    };
    typedef void* (*Allocator_Proc)(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data);
    struct Allocator {
        Allocator_Proc proc;
        void* data;
    };
    
    // Memory management procedures
    void* allocate(usize size, Allocator allocator);
    void* resize(void* memory, usize size, usize old_size, Allocator allocator);
    void  dispose(void* memory, Allocator allocator);
    template <typename T> T* allocate(usize num = 1, Allocator allocator = {}) { return (T*)allocate(sizeof(T) * num, allocator); }
    template <typename T> T* make(Allocator allocator) {
        auto mem = (T*)allocate(sizeof(T), allocator);
        *mem     = T();
        return mem;
    }

    // Page Allocator
    struct Page {
        void* memory;
        u64 size;
    };
    
    Page _os_get_page(u64 size);
    void _os_del_page(Page page);
    // Page _os_reserve_page(Page page);
    // Page _os_commit_page(Page page, u64 extra);
    
#ifdef DEVELOPER
    Page traced_os_get_page(Source_Location loc, u64 size);
    void traced_os_del_page(Source_Location loc, Page page);
    #define os_get_page(...) ::ncz::traced_os_get_page(::ncz::Source_Location {__FILE__, __LINE__}, __VA_ARGS__)
    #define os_del_page(...) ::ncz::traced_os_del_page(::ncz::Source_Location {__FILE__, __LINE__}, __VA_ARGS__)
#else
    #define os_get_page ::ncz::_os_get_page
    #define os_del_page ::ncz::_os_del_page
#endif
    
    // Array Types
    template <typename T>
    struct Array {
        T*    data  = nullptr;
        usize count = 0;
        
        T& operator[](usize index) {
            assert(index >= 0);
            assert(index < count);
            return data[index];
        }
        
        const T& operator[](usize index) const {
            assert(index >= 0);
            assert(index < count);
            return data[index];
        }
        
        T* begin() const { return data; }
        T* end()   const { return data + count; }
        
        
        T* advance() {
            assert(data);
            if (!count) return nullptr;
            count -= 1;
            return data++;
        }
        
        usize contains(T val) {
            usize count = 0;
            for (usize i = 0; i < count; ++i) if (data[i] == val) count += 1;
            return count;
        }
        
        bool has(T val) {
            for (usize i = 0; i < count; ++i) if (data[i] == val) return true;
            return false;
        }
        
        usize unordered_remove_by_value(T val) {
            usize removed = 0;
            for (usize i = 0; i < count; ++i) if (data[i] == val) {
                removed += 1;
                data[i]  = data[--count];
                i -= 1;
            }
            return removed;
        }
        
        void unordered_remove_by_index(s64 index) {
            assert(index >= 0);
            assert(index < this->count);
            this->data[index] = this->data[--this->count];
        }
        
        void ordered_remove_by_index(s64 index) {
            assert(index >= 0);
            assert(index < this->count);
            memmove(&this->data[index], &this->data[index + 1], sizeof(T)*(this->count-index));
            this->count -= 1;
        }
    };
    
    struct String : Array<char> {
        String();
        String(char* d, usize c);
        String chop_by(char delim);
    };
    
    String operator ""_str(cstr data, usize count);
    String from_cstr(cstr s);
    
    template <typename T, usize capacity>
    struct Fixed_List {
        usize count = 0;
        T data[capacity];
        
        T* push(T item) { assert(capacity > count);  data[count++] = item; return data+count-1; }
        T pop() { assert(count > 0); return data[--count]; }
        const Array<T> as_array() const { return { static_cast<T*>(data), count }; }
        Array<T> as_array() { return { static_cast<T*>(data), count }; }
        template <typename ... Ts> void append(Ts ... args) { (push(args), ...); }
        void append(Array<T> ts) {
            assert(count + ts.count < capacity);
            memcpy(data+count, ts.data, ts.count*sizeof(T));
            count += ts.count;
        }
        T* begin() const { return (T*)(data); }
        T* end()   const { return (T*)(data) + count; }
    };
    
    // Logging
    enum class Log_Level {
        NORMAL  = 0,
        VERBOSE = 1,
        TRACE   = 2,
    };
    
    enum class Log_Type {
        INFO = 0,
        ERRO = 1,
        WARN = 2,
    };
    
    typedef void (*Logger_Proc)(String message, Log_Level level, Log_Type type, void* logger_data);
    
    #ifndef NCZ_NUM_LOG_LABELS
    #define NCZ_NUM_LOG_LABELS 128
    #endif
    struct Logger {
        Logger_Proc proc;
        void* data;
        Fixed_List<cstr, NCZ_NUM_LOG_LABELS> labels;
    };
    
    // C Runtime Logger
    void crt_logger_proc(String message, Log_Level level, Log_Type type, void* logger_data);
    extern Logger crt_logger;
    
    #pragma region Allocators

    // Unmapping Allocator
    Allocator get_unmapping_allocator();

    // C Runtime Allocator
    void* crt_allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data);
    extern Allocator crt_allocator;
    
    
    // Pool Allocator
#ifndef ARENA_ALIGN
    #define ARENA_ALIGN 8
#endif
    void* pool_allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data);
    struct Pool {
        struct Marker {
            void** block;
            usize  offset;
        };
        Marker    mark;
        Allocator block_allocator;
        usize     block_size;
        
        // linked lists of memory blocks where we embed the next pointer at the beginning of the block
        void** blocks;
        void** oversized;

        Pool(usize block_size, Allocator block_allocator);
        Allocator allocator();
        void* get(usize bytes);
        void  reset();   // clears normal blocks for reuse and disposes of any oversized blocks
        void  dispose(); // disposes of all memory blocks
    };  

    // Flat Pool Allocator
    void* flat_pool_allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data);
    
    
    struct Flat_Pool {
        u8* current_point;
        u8* memory_base;
        u8* first_uncommitted_page;
        u8* address_limit;
        
        Flat_Pool(u64 reserve_size);
        Allocator allocator();
        void* get(u64 num_bytes);
        void  reset();
        void  dispose();
    };
    
    template <usize CAPACITY>
    struct Fixed_Pool {
         Fixed_List<u8, CAPACITY> data = {};
         void* get(usize num_bytes) {
            assert(data.count + num_bytes <= CAPACITY);
            void* mem = &data.data[data.count];
            data.count += num_bytes;
            return mem;   
         }
         void reset() { data.count = 0; }
         
         static void* allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data) {
                auto pool = static_cast<Fixed_Pool<CAPACITY>*>(allocator_data);
                assert(pool != nullptr);
                switch (mode) {
                case Allocator_Mode::ALLOCATE: return pool->get(size);
                case Allocator_Mode::DISPOSE:  return nullptr;
                case Allocator_Mode::RESIZE: {
                    void* new_memory = pool->get(size);
                    memcpy(new_memory, old_memory, old_size);
                    return new_memory;
                }
                }
                
                assert(false && "unreachable");
                return nullptr;
         }
         
         Allocator allocator() { return { allocator_proc, static_cast<void*>(this) }; }
    };
    
#pragma endregion
    
        // Allocator allocator         = get_unmapping_allocator();
    // Context
    struct Context {
        Allocator allocator         = crt_allocator;
        Logger    logger            = crt_logger;
        Pool      temporary_storage = Pool(32 * 1024, crt_allocator);
        bool      handling_assert   = false;
    };
    
    extern thread_local Context context;
    
    // Dynamic Data Structures
    
    template <typename T>
    struct List : Array<T> {
        usize     capacity;
        Allocator allocator;
        
        List()
            : capacity(0), allocator({}) {}
        List(Allocator a)
            : capacity(0), allocator(a) {};
    
        void expand() {
            if (!allocator.proc) allocator = context.allocator;
            auto new_capacity = this->data ? 2 * capacity : 256;
            this->data = static_cast<T*>(
                resize(this->data, sizeof(T)*new_capacity, sizeof(T)*capacity, allocator)
            );
            capacity = new_capacity;
        }
        
        void push(T item) {
            if (this->count >= capacity) expand();
            this->data[this->count++] = item;
        }
        
        T pop() {
            assert(this->count > 0);
            return this->data[--this->count];
        }
        
        template <typename ... Ts>
        void append(Ts ... args) { (push(args), ...); }
        void append(Array<T> ts) {
            while (this->count + ts.count < capacity) expand();
            memcpy(this->data+this->count, ts.data, ts.count+sizeof(T));
            this->count += ts.count;
        }
    };
    
    using String_Builder = List<char>;
    
    #ifndef NCZ_BUCKET_ARRAY_DEFAULT_ITEMS_PER_BUCKET
    #define NCZ_BUCKET_ARRAY_DEFAULT_ITEMS_PER_BUCKET 256
    #endif
    
    
    template <typename T, u32 ITEMS_PER_BUCKET = NCZ_BUCKET_ARRAY_DEFAULT_ITEMS_PER_BUCKET>
    struct Bucket_Array {
        static_assert(ITEMS_PER_BUCKET % 64 == 0, "");
        
        struct Bucket {
            T   data     [ITEMS_PER_BUCKET];
            u64 occupied [ITEMS_PER_BUCKET/64];
            u32 bucket_index;
            u32 count;
        };
        
        struct Index {
            u32 bucket_index = 0;
            u32 item_index   = 0;
        };
        
        usize         count;
        List<Bucket*> all_buckets;
        List<Bucket*> unfull_buckets;
        
        Bucket_Array()
            : count(0), all_buckets(context.allocator), unfull_buckets(context.allocator) {}
        Bucket_Array(Allocator a)
            : count(0), all_buckets(a), unfull_buckets(a) {}
        
        T& operator[](Index i) {
            assert(i.item_index >= 0);
            assert(i.item_index <  ITEMS_PER_BUCKET);
            assert(occupied(i));
            return all_buckets[i.bucket_index]->data[i.item_index];
        }
        
        const T& operator[](Index i) const {
            assert(i.item_index >= 0);
            assert(i.item_index <  ITEMS_PER_BUCKET);
            assert(occupied(i));
            return all_buckets[i.bucket_index]->data[i.item_index];
        }
        
        bool occupied(Index index) {
            u32 n = all_buckets[index.bucket_index]->occupied[index.item_index/64];
            u32 k = index.item_index%64;
            return n & (1 << k);
        }
            
        Bucket* add_bucket() {
            assert(all_buckets.allocator.proc);
            assert(unfull_buckets.allocator.proc);
            assert(unfull_buckets.count == 0);
            
            auto new_bucket = allocate<Bucket>(1, all_buckets.allocator);
            memset(new_bucket, 0, sizeof(Bucket));
            new_bucket->bucket_index = static_cast<u32>(all_buckets.count);
            
            all_buckets.push(new_bucket);
            unfull_buckets.push(new_bucket);
            return new_bucket;
        }
        
        u32 nlz(u64 x) {
            u32 y, m, n;
            
            y = -(x >> 16);      // If left half of x is 0, 
            m = (y >> 16) & 16;  // set n = 16. If left half 
            n = 16 - m;          // is nonzero, set n = 0 and 
            x = x >> m;          // shift x right 16. 
                                 // Now x is of the form 0000xxxx. 
            y = x - 0x100;       // If positions 8-15 are 0, 
            m = (y >> 16) & 8;   // add 8 to n and shift x left 8. 
            n = n + m; 
            x = x << m; 
            
            y = x - 0x1000;      // If positions 12-15 are 0, 
            m = (y >> 16) & 4;   // add 4 to n and shift x left 4. 
            n = n + m; 
            x = x << m; 
            
            y = x - 0x4000;      // If positions 14-15 are 0, 
            m = (y >> 16) & 2;   // add 2 to n and shift x left 2. 
            n = n + m; 
            x = x << m;
            
            y = x >> 14;         // Set y = 0, 1, 2, or 3. 
            m = y & ~(y >> 1);   // Set m = 0, 1, 2, or 2 resp.
            return n + 2 - m; 
        }
        
        Index get() {
            if (!unfull_buckets.count) add_bucket();
            Bucket* bucket = unfull_buckets[0];
            
            for (u32 i = 0; i < ITEMS_PER_BUCKET/64; ++i) {
                u32 n = nlz(~bucket->occupied[i]);
                if (n < 64) {
                     bucket->occupied[i] = (static_cast<u64>(1) << static_cast<u64>(n)) | bucket->occupied[i];
                     bucket->data[n]     = T();
                     bucket->count      += 1;
                     count += 1;
                     if (bucket->count >= ITEMS_PER_BUCKET) {
                        usize removed = unfull_buckets.unordered_remove_by_value(bucket);
                        assert(removed == 1);
                     }
                     return { bucket->bucket_index, i * 64 + n };
                }
            }
            
            assert(false && "unreachable");
            return {};
        }
        
        void del(Index l) {
            auto bucket = all_buckets[l.bucket_index];
            assert(occupied(l));
            bool was_full = bucket->count == ITEMS_PER_BUCKET;
            bucket->occupied[l.item_index] = false;
            bucket->count -= 1;
            this->count   -= 1;
            if (was_full) {
                assert(!unfull_buckets.has(bucket));
                unfull_buckets.push(bucket);
            }
        }
        
        void push(T t) {
            Index l = get();
            all_buckets[l.bucket_index]->data[l.item_index] = t;
        }
        
        void reset() {
            count = 0;
            unfull_buckets.reset();
            for (auto* b : all_buckets) {
                memset(b->occupied, 0, sizeof(bool) * ITEMS_PER_BUCKET);
                b->count = 0;
                unfull_buckets.push(b);
            }
        }
        
        void dispose() {
            for (auto* b : all_buckets) dispose(b, all_buckets.allocator);
            count = 0;
            all_buckets.dispose();
            unfull_buckets.dispose();
        }
        
        struct Iterator {
            Index index;
            const Array<Bucket*>* buckets;
            
            Iterator(const Array<Bucket*>* b, Index i)
                : index(i), buckets(b) {}
            
            bool operator!=(Iterator other) {
                return index.item_index   != other.index.item_index
                    || index.bucket_index != other.index.bucket_index;
            }
            
            Iterator& operator++() {
                do {
                    index.item_index += 1;
                    if (index.item_index >= (*buckets)[index.bucket_index]->count) {
                        index.bucket_index += 1;
                        index.item_index    = 0;
                        if (index.bucket_index == buckets->count) break;
                    }
                } while (!(*buckets)[index.bucket_index]->occupied[index.item_index]);
                return *this;
            }
            
            T& operator*() {
                auto b = (*buckets)[index.bucket_index];
                assert(index.item_index >= 0);
                assert(index.item_index < static_cast<u32>(b->count));
                return b->data[index.item_index];
            }
            
            const T& operator*() const {
                auto b = (*buckets)[index.bucket_index];
                assert(index.item_index >= 0);
                assert(index.item_index < static_cast<u32>(b->count));
                return b->data[index.item_index];
            }
            
            T* operator->() {
                auto b = (*buckets)[index.bucket_index];
                assert(index.item_index >= 0);
                assert(index.item_index < static_cast<u32>(b->count));
                return &b->data[index.item_index];
            }
        };
        
        Iterator begin() const { return Iterator(&all_buckets, {0, 0}); }
        Iterator end()   const { return Iterator(&all_buckets,
            {static_cast<u32>(all_buckets.count), 0}
        );}
    };
    
    /* // OOPS NOT DONE
    struct Slab {
        void* page = nullptr;
        void* free = nullptr;
        Slab* prev = nullptr;
        Slab* next = nullptr;
    };
    
    struct Slab_Cache {
        Slab* partial = nullptr;
        Slab* empty   = nullptr;
        Slab* full    = nullptr;
        void* get(usize obj_size);
        void del(usize obj_size, void* p);
        void reap();
        void reset();
        void dispose();
    };
    
    template <typename T, usize PAGES_PER_SLAB>
    struct Slab_Array {
        struct Slab {
            void* base = nullptr;
            void* free = nullptr;
            Slab* prev = nullptr;
            Slab* next = nullptr;
        };
        
        static_assert(sizeof(T) >= 2*sizeof(usize))
        static_assert(sizeof(T) < PAGES_PER_SLAB*NCZ_DEFAULT_PAGE_SIZE - sizeof(Slab));
        
        Slab* partial = nullptr;
        Slab* empty   = nullptr;
        Slab* full    = nullptr;
        usize count   = 0;
        
        T* get();
        void del(T* obj);
    };
    */
    
    void format(String_Builder* sb, String str);
    void format(String_Builder* sb, String_Builder str);

    u64 hash(Array<u8> bytes);
    template <typename T> Array<u8> as_bytes(T* data) { return { (u8*) data, sizeof(T) }; }
    template <typename T> u64 hash(T t) { return hash(as_bytes(&t)); }
    template <typename Key, typename Value>
    struct Table {
        struct Bucket { Key key; Value value; bool occupied; };
        Bucket* buckets     = nullptr;
        usize count         = 0;
        usize capacity      = 0;
        Allocator allocator = {};
        
        void expand() {
            const int INITIAL_CAPACITY = 256;
            if (!buckets) {
                assert(capacity == 0);
                assert(count == 0);
                if (!allocator.proc) allocator = context.allocator;
                buckets  = allocate<Bucket>(INITIAL_CAPACITY, allocator);
                memset(buckets, 0, INITIAL_CAPACITY*sizeof(Bucket));
                capacity = INITIAL_CAPACITY;
            } else {
                Table<Key, Value> new_map {
                    allocate<Bucket>(capacity * 2, allocator),
                    0, capacity * 2, allocator
                };

                memset(new_map.buckets, 0, new_map.capacity * sizeof(Bucket));

                for (usize i = 0; i < capacity; ++i) if (buckets[i].occupied) {
                    new_map.insert(buckets[i].key, buckets[i].value);
                }

                dispose(buckets, allocator);
                *this = new_map;
            }
        }
        void insert(Key key, Value value) {
            if (count >= capacity) expand();
            u64 h = hash(key) & (capacity - 1);
            for (usize i = 0; i < capacity && buckets[h].occupied && buckets[h].key != key; ++i)
                h = (h + 1) & (capacity - 1);
            assert(!buckets[h].occupied || buckets[h].key == key);
            // TODO: collision detection and return Maybe<Value>??? bool old_value = buckets[h].key == key
            if (!buckets[h].occupied) count += 1;
            buckets[h].occupied = true;
            buckets[h].key      = key;
            buckets[h].value    = value;
        }
        bool remove(Key key) {
            assert(buckets);
            u64 h = hash(key) & (capacity - 1);
            for (usize i = 0; i < capacity && buckets[h].occupied && buckets[h].key != key; ++i)
                h = (h + 1) & (capacity - 1);
            if (buckets[h].occupied && buckets[h].key == key) {
                buckets[h].occupied = false;
                return true;
            } else return false;
        }
        Value* get(Key key) {
            if (!buckets) return nullptr;
            u64 h = hash(key) & (capacity - 1);
            for (usize i = 0; i < capacity && buckets[h].occupied && buckets[h].key != key; ++i)
                h = (h + 1) & (capacity - 1);
            if (buckets[h].occupied && buckets[h].key == key)
                return &buckets[h].value;
            else
                return nullptr;
        }
    };
    
    struct Unmapping_Allocator {
        Table<void*, u64> pages;
        // Mutex lock;
    };
    
    void* unmapping_allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data);
#pragma endregion    
#pragma region Formatting
    // nicer snprintf
    template <typename ... Args>
    String print(Allocator allocator, Args ... args) {
        String_Builder sb(allocator);
        (format(&sb, args), ...);
        sb.push('\0'); // when in Rome...
        return { sb.data, sb.count-1 };
    }
    template <typename ... Args>
    String cprint(Args ... args) {
        String_Builder sb(context.allocator);
        (format(&sb, args), ...);
        sb.push('\0'); // when in Rome...
        return { sb.data, sb.count-1 };
    }
    template <typename ... Args>
    String tprint(Args ... args) {
        String_Builder sb(NCZ_TEMP);
        (format(&sb, args), ...);
        sb.push('\0'); // when in Rome...
        return { sb.data, sb.count-1 };
    }
    
    template <typename T>
    void format(String_Builder* sb, Array<T> list) {
        sb->push('[');
        for (usize i = 0; i < list.count; ++i) {
            format(sb, list[i]);
            if (i != list.count - 1) format(sb, ", "_str);
        }
        sb->push(']');
    }
    template <typename T>
    void format(String_Builder* sb, List<T> list) {
        sb->push('[');
        for (usize i = 0; i < list.count; ++i) {
            format(sb, list[i]);
            if (i != list.count - 1) format(sb, ", "_str);
        }
        sb->push(']');
    }

    template <typename Key, typename Value>
    void format(String_Builder* sb, Table<Key, Value> map) {
        sb->push('{');
        bool f = true;
        for (auto* it = map.buckets; it != map.buckets + map.capacity; ++it) {
            if (!it->occupied) continue;
            if (f) f = false; else format(sb, ", "_str);
            write(sb, it->key, ": "_str, it->value);
        }
        sb->push('}');
    }
    
    void format(String_Builder* sb, u8  x);
    void format(String_Builder* sb, s8  x);
    void format(String_Builder* sb, u16 x);
    void format(String_Builder* sb, s16 x);
    void format(String_Builder* sb, u32 x);
    void format(String_Builder* sb, s32 x);
    void format(String_Builder* sb, u64 x);
    void format(String_Builder* sb, s64 x);
    void format(String_Builder* sb, cstr   x);
    void format(String_Builder* sb, void*  x);
    void format(String_Builder* sb, float  x);
    void format(String_Builder* sb, double x);

    template <typename ... Args> void print(String_Builder* sb, Args ... args) { (format(sb, args), ...); }
    
#pragma endregion
#pragma region Logging
    // Logging
    #define log(...)       log_ex(::ncz::Log_Level::NORMAL,  ::ncz::Log_Type::INFO, __VA_ARGS__)
    #define log_info(...)  log_ex(::ncz::Log_Level::VERBOSE, ::ncz::Log_Type::INFO, __VA_ARGS__)
    #define log_error(...) log_ex(::ncz::Log_Level::NORMAL,  ::ncz::Log_Type::ERRO, __VA_ARGS__)
    #define log_warn(...)  log_ex(::ncz::Log_Level::WARN,    ::ncz::Log_Type::ERRO, __VA_ARGS__)
    
    #define show(expr) TODO_VERBOSE_INFO_AND_STRINGIFY
    
    template <typename ...Args>
    void log_ex(Log_Level level, Log_Type type, Args... args) {
        auto mark = context.temporary_storage.mark;
            String_Builder sb {};
            sb.allocator = NCZ_TEMP;
            for (cstr label : context.logger.labels) {
                sb.push('[');
                format(&sb, label);
                sb.append(']', ' ');
            }
            (format(&sb, args), ...);
            sb.append('\n', '\0'); // when in Rome...
            context.logger.proc({sb.data, sb.count - 1}, level, type, context.logger.data);
        context.temporary_storage.mark = mark;
    }
    
#pragma endregion

#pragma region Multiprocessing
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
struct Process {
    HANDLE  proc;
    HANDLE output;
};
// typedef HANDLE Process;
#else
struct Process { int proc; };
#endif // _WIN32

bool wait(Process proc);
bool wait(Array<Process> proc);
Process run_command_async(Array<cstr> args, bool trace = true);
bool run_command_sync(Array<cstr> args, bool trace = true);

// runs a system command, the arguments should be a variable number
// of null terminated c strings, but c++ varargs do not have the
// technology to express this without drinking a lethal does of template coolaid,
// so if you pass something that isn't a cstr you will get an error saying something like:
// "we couldn't convert the type you passed to a cstr when we called push"
// instead of a normal typecheck
template <typename ... Args>
bool run_cmd(Args ... args) {
    auto mark = context.temporary_storage.mark;
    List<cstr> cmd(NCZ_TEMP);
    (cmd.push(args), ...);
    bool ok = run_command_sync(cmd);
    context.temporary_storage.mark = mark;
    return ok;
}

#pragma endregion
#pragma region FileSystem
bool needs_update(cstr output_path, Array<cstr> input_paths);
bool needs_update(cstr output_path, cstr input_paths);
bool rename_file(cstr old_path, cstr new_path);
#pragma endregion
#pragma region Misc
    // Utililty Templates
    template <typename T>
    void clamp(T* t, T min, T max) {
        if (*t < min) *t = min;
        else if (*t > max) *t = max;
    }
    template <typename T>
    T clamp(T t, T min, T max) {
        if (t < min) return min;
        else if (t > max) return max;
        else              return t;
    }
    template <usize limit>
    Fixed_Pool<limit>* use_global_arena() {
        static Fixed_Pool<limit> global_arena {};
        auto allocator    = global_arena.allocator();
        context.allocator = allocator;
        context.temporary_storage.block_allocator = allocator;
        return &global_arena;
    }
    void maybe_reload_cpp_script(Array<cstr> args, cstr src);
    String to_string(cstr c);
#pragma endregion
}
#endif // NCZ_HPP_

#ifdef NCZ_IMPLEMENTATION
static constexpr u64 align(u64 n, u64 a) { return (n + a-1) & ~(a-1); }
namespace ncz {

#ifdef _WIN32
#include "ncz_win32.cpp"
#else
#include "ncz_posix.cpp"
#endif

    thread_local Context context {};
    
    void failed_assert(cstr repr, Source_Location loc) {
        if (context.handling_assert) return;
        context.handling_assert = true;
        log_error(loc, ": Assertion `", repr, "` Failed!");
        log_stack_trace(2);
        exit(1); // TODO
    }
    
    String::String()
        : Array<char> {} {}
    String::String(char* d, usize c)
        : Array<char> { d, c } {}
    String String::chop_by(char delim) {
        for (char* it = data; it != data + count; ++it) if (*it == delim) {
            usize diff = it - data;
            String lhs(data, diff);
            *this = { data + diff + 1, count - diff - 1};
            return lhs;
        }
        return {};
    }
    
    String to_string(cstr c) {
        return String { (char*)c, strlen(c) };
    }
    
    bool needs_update(cstr output_path, cstr input_paths) {
        return needs_update(output_path, { &input_paths, 1 });
    }
    
    void maybe_reload_cpp_script(Array<cstr> args, cstr src) {
        if (needs_update(args[0], src)) {
            cstr old = tprint(args[0], ".old").data;
            if (!rename_file(args[0], old)) exit(1);
            if (!run_cmd(NCZ_CC(args[0], src))) {
                rename_file(old, args[0]);
                exit(1);
            }
            if (!run_command_sync(args)) exit(1);
            exit(0);
        }
    }
    
    // Memory Management
    
    void* allocate(usize size, Allocator allocator = {}) {
        if (!size) return nullptr;
        Allocator a = allocator.proc != nullptr ? allocator : context.allocator;
        assert(a.proc != nullptr);
        void* mem = a.proc(Allocator_Mode::ALLOCATE, size, 0, nullptr, a.data);
        // You can do custom checks and logging here!!
        return mem;
    }
    
    void* resize(void* memory, usize size, usize old_size, Allocator allocator = {}) {
        Allocator a = allocator.proc != nullptr ? allocator : context.allocator;
        assert(a.proc != nullptr);
        void* mem = a.proc(Allocator_Mode::RESIZE, size, old_size, memory, a.data);
        // You can do custom checks and logging here!!
        return mem;
    }
    
    void dispose(void* memory, Allocator allocator = {}) {
        Allocator a = (allocator.proc != nullptr) ? allocator : context.allocator;
        assert(a.proc != nullptr);
        a.proc(Allocator_Mode::DISPOSE, 0, 0, memory, a.data);
        // You can do custom checks and logging here!!
    }

    void* unmapping_allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data) {
        assert(allocator_data);
        auto allocator = (Unmapping_Allocator*)allocator_data;
        switch (mode) {
        case Allocator_Mode::ALLOCATE: {
            Page p = os_get_page(size);
            allocator->pages.insert(p.memory, p.size);
            return p.memory;
        } break;
        case Allocator_Mode::RESIZE: {
            u64* old_page_size = allocator->pages.get(old_memory);
            if (old_page_size) {
                if (*old_page_size >= size) return old_memory;
                Page p = os_get_page(size);
                allocator->pages.remove(old_memory);
                memcpy(p.memory, old_memory, old_size);
                allocator->pages.insert(p.memory, p.size);
                return p.memory;
            } else {
                assert(old_memory == nullptr);
                Page p = os_get_page(size);
                allocator->pages.insert(p.memory, p.size);
                return p.memory;
            }
        } break;
        case Allocator_Mode::DISPOSE: {
            u64* s = allocator->pages.get(old_memory);
            assert(s);
            os_del_page({ old_memory, *s });
            allocator->pages.remove(old_memory);
            return nullptr;
        } break;
        }
        return nullptr;
    }
    
    Logger    crt_logger    { crt_logger_proc,    nullptr, {} };
    Allocator crt_allocator { crt_allocator_proc, nullptr };

    Allocator get_unmapping_allocator() {
        Flat_Pool bootstrap_pool(32 * 1024 * 1024);
        auto bootstrap_allocator = bootstrap_pool.allocator();
        auto unmapping_allocator = allocate<Unmapping_Allocator>(1, bootstrap_allocator);
        auto pool                = allocate<Flat_Pool>(1, bootstrap_allocator);
        *pool = bootstrap_pool;
        unmapping_allocator->pages = {};
        unmapping_allocator->pages.allocator = {flat_pool_allocator_proc, pool};
        return { unmapping_allocator_proc, unmapping_allocator };
    }


    // C Runtime Allocator
    void* crt_allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data) {
        (void) allocator_data;
        (void) old_size;
        switch (mode) {
        case Allocator_Mode::ALLOCATE: return malloc(size);
        case Allocator_Mode::RESIZE:   return realloc(old_memory, size);
        case Allocator_Mode::DISPOSE:         free(old_memory);
        }
        return nullptr;
    }

    // Pool Allocator
    Pool::Pool(usize size, Allocator a = {}) {
        block_size      = size;
        block_allocator = a.proc ? a : context.allocator;
        blocks          = (void**) allocate(sizeof(void*) + block_size, block_allocator);
        *blocks         = nullptr;
        oversized       = nullptr;
        mark            = { blocks, 0 };
    }
    
    
    Allocator Pool::allocator() { return { pool_allocator_proc, static_cast<void*>(this) }; }
    
    
    void* Pool::get(usize bytes) {
        assert(block_allocator.proc);
        assert(blocks);
        assert(mark.block);

        if (bytes <= block_size) {
            u8* current_block_data = ((u8*)mark.block)+sizeof(void*);
            u8* memory             = (u8*)(((u64)((current_block_data+mark.offset) + ARENA_ALIGN-1)) & ((u64)~(ARENA_ALIGN - 1)));
            u8* current_block_end  = current_block_data + block_size;

            if (memory > current_block_end || memory + bytes > current_block_end) {
                // we need to cycle in a new block
                if (!*(mark.block)) {
                    *mark.block = allocate(sizeof(void*) + block_size + ARENA_ALIGN - 1, block_allocator);
                    *((void**)*mark.block) = nullptr;
                }
                mark = { (void**)*mark.block, 0 };
                current_block_data = ((u8*)mark.block) + sizeof(void*);
                memory = (u8*)(((u64)((current_block_data) + ARENA_ALIGN - 1)) & ((u64)~(ARENA_ALIGN - 1)));
                current_block_end = current_block_data + block_size;
            }

            assert(memory < current_block_end);
            assert(memory + bytes <= current_block_end);
            assert(memory >= current_block_data);

            mark.offset = (memory + bytes) - current_block_data;
            return memory;
        } else {
            auto new_block = (void**)allocate(sizeof(void*)+bytes+ARENA_ALIGN-1);
            *new_block = oversized;
            oversized = new_block;
            return &new_block[1];
        }
    }
    void  Pool::reset() {
        mark = { blocks, 0 };
        for (void** block = oversized; block != nullptr; block = (void**)*block) {
            ncz::dispose(block, block_allocator);
        }
#ifdef DEVELOPER
        for (void** block = blocks; block != nullptr; block = (void**)*block) {
            memset(&block[1], 0xcd, block_size);
        }
#endif
    }
    void  Pool::dispose() {
        for (void** block = blocks; block != nullptr; block = (void**)*block) {
            ncz::dispose(block, block_allocator);
        }
        for (void** block = oversized; block != nullptr; block = (void**)*block) {
            ncz::dispose(block, block_allocator);
        }
    }
    void* pool_allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data) {
        auto pool = (Pool*)allocator_data;
        assert(pool != nullptr);
        switch (mode) {
        case Allocator_Mode::ALLOCATE: return pool->get(size);
        case Allocator_Mode::DISPOSE:  return nullptr;
        case Allocator_Mode::RESIZE: {
            void* new_memory = pool->get(size);
            memcpy(new_memory, old_memory, old_size);
            return new_memory;
        }
        }
        return nullptr;
    }

    // Flat Pool Allocator

#ifdef DEVELOPER
    Page traced_os_get_page(Source_Location loc, u64 size) {
        Page page = _os_get_page(size);
        log_ex(Log_Level::TRACE, Log_Type::INFO, loc,
               " Mapped "_str, page.size/1024, "Kb of virtual memory ("_str, page.memory, ")."_str);
        return page;
    }
    void traced_os_del_page(Source_Location loc, Page page) {
        _os_del_page(page);
        log_ex(Log_Level::TRACE, Log_Type::INFO, loc,
            " Unmapped  "_str, page.size/1024, "Kb of virtual memory ("_str, page.memory, ")."_str);
    }
#endif


    Flat_Pool::Flat_Pool(u64 reserve_size) {
        reserve_size           = align(reserve_size, NCZ_PAGE_SIZE);
        memory_base            = os_reserve_pages(reserve_size);
        current_point          = memory_base;
        first_uncommitted_page = memory_base;
        address_limit          = memory_base + reserve_size;
    }
    Allocator Flat_Pool::allocator() { return { flat_pool_allocator_proc, (void*)this }; }
    void* Flat_Pool::get(u64 num_bytes) {
        u8* result = (u8*) align((u64)current_point, ARENA_ALIGN);
        u8* end    = result + num_bytes;
        if (end > first_uncommitted_page) {
            assert(end <= address_limit && "Flat_Pool IS FULL!!!!");
            first_uncommitted_page = os_extend_commited_pages(first_uncommitted_page, end);
        }
        current_point = end;
        return result;
    }
    void  Flat_Pool::reset() {
        
    }
    void  Flat_Pool::dispose() {

    }
    void* flat_pool_allocator_proc(Allocator_Mode mode, usize size, usize old_size, void* old_memory, void* allocator_data) {
        auto pool = (Flat_Pool*)allocator_data;
        assert(pool != nullptr);
        switch (mode) {
        case Allocator_Mode::ALLOCATE: return pool->get(size);
        case Allocator_Mode::DISPOSE:  return nullptr;
        case Allocator_Mode::RESIZE: {
            void* new_memory = pool->get(size);
            memcpy(new_memory, old_memory, old_size);
            return new_memory;
        }
        }
        return nullptr;
    }
    
    String operator ""_str(cstr data, usize count) { return { (char*) data, count }; }
    String from_cstr(cstr s) { 
        String str;
        str.count = strlen(s);
        str.data  = (char*) s;
        return str;
    }
    
    // Multiprocessing
    bool run_command_sync(Array<cstr> args, bool trace) {
        auto proc = run_command_async(args, trace);
        if (!proc.proc) return false;
        return wait(proc);
    }
    
    bool wait(Array<Process> procs) {
        bool ok = true;
        for (auto proc : procs) {
            ok = wait(proc) && ok;
        }
        return ok;
    }
    
        
    // Base Hash Imlementation
    u64 hash(Array<u8> bytes) {
        u64 hash = 5381;
        for (usize i = 0; i < bytes.count; ++i) hash = ((hash << 5) + hash) + bytes.data[i];
        return hash;
    }
    
    void format(String_Builder* sb, String_Builder str) { format(sb, String { str.data, str.count }); }
    void format(String_Builder* sb, String str) {
        while (sb->capacity - sb->count < str.count) sb->expand();
        memcpy(sb->data+sb->count, str.data, str.count);
        sb->count += str.count;
    }

    // Format Implementations
    void format(String_Builder* sb, Source_Location loc) {
        format(sb, loc.file);
        sb->push(':');
        format(sb, loc.line);
    }
    
    void format(String_Builder* sb, cstr s) {
        auto len = strlen(s);
        while (sb->capacity < sb->count + len) sb->expand();
        assert(sb->capacity >= sb->count + len);
        memcpy(sb->data + sb->count, s, len);
        sb->count += len;
    }
    
    void format(String_Builder* sb, void* p) {
        constexpr const auto MAX_LEN = 32;
        char buf[MAX_LEN];
        auto len = snprintf(buf, MAX_LEN, "%p", p);
        assert(len > 0);
        while (sb->capacity < sb->count + len) sb->expand();
        assert(sb->capacity >= sb->count + len);
        memcpy(sb->data + sb->count, buf, len);
        sb->count += len;
    }
    
    void format(String_Builder* sb, f64 f) { format(sb, static_cast<f32>(f)); }
    void format(String_Builder* sb, f32 f) {
        constexpr const auto MAX_LEN = 32;
        char buf[MAX_LEN];
        auto len = snprintf(buf, MAX_LEN, "%f", f);
        assert(len > 0);
        while (sb->capacity < sb->count + len) sb->expand();
        assert(sb->capacity >= sb->count + len);
        memcpy(sb->data + sb->count, buf, len);
        sb->count += len;
    }
    
    void format(String_Builder* sb, u8  x) { format(sb, static_cast<u64>(x)); }
    void format(String_Builder* sb, u16 x) { format(sb, static_cast<u64>(x)); }
    void format(String_Builder* sb, u32 x) { format(sb, static_cast<u64>(x)); }
    void format(String_Builder* sb, u64 x) {
        constexpr const auto MAX_LEN = 32;
        char buf[MAX_LEN];
        auto len = snprintf(buf, MAX_LEN, "%" PRIu64, x);
        assert(len > 0);
        while (sb->capacity < sb->count + len) sb->expand();
        assert(sb->capacity >= sb->count + len);
        memcpy(sb->data + sb->count, buf, len);
        sb->count += len;
    }
    
    void format(String_Builder* sb, s8  x) { format(sb, static_cast<s64>(x)); }
    void format(String_Builder* sb, s16 x) { format(sb, static_cast<s64>(x)); }
    void format(String_Builder* sb, s32 x) { format(sb, static_cast<s64>(x)); }
    void format(String_Builder* sb, s64 x) {
        constexpr const auto MAX_LEN = 32;
        char buf[MAX_LEN];
        auto len = snprintf(buf, MAX_LEN, "%" PRId64, x);
        assert(len > 0);
        while (sb->capacity < sb->count + len) sb->expand();
        assert(sb->capacity >= sb->count + len);
        memcpy(sb->data + sb->count, buf, len);
        sb->count += len;
    }
}
#endif // NCZ_IMPLEMENTATION