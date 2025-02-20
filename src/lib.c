#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef _MSC_VER // sigh...
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

#include "nasal.h"
#include "code.h"

#define NEWSTR(c, s, l) naStr_fromdata(naNewString(c), s, l)
#define NEWCSTR(c, s) NEWSTR(c, s, strlen(s))

// Generic argument error, assumes that the symbol "c" is a naContext,
// and that the __FUNCTION__ string is of the form "f_NASALSYMBOL".
#define ARGERR() \
    naRuntimeError(c, "bad/missing argument to %s()", (__FUNCTION__ + 2))

static naRef f_size(naContext c, naRef me, int argc, naRef* args)
{
    if(argc == 0) ARGERR();
    if(naIsString(args[0])) return naNum(naStr_len(args[0]));
    if(naIsVector(args[0])) return naNum(naVec_size(args[0]));
    if(naIsHash(args[0])) return naNum(naHash_size(args[0]));
    naRuntimeError(c, "object has no size()");
    return naNil();
}

static naRef f_keys(naContext c, naRef me, int argc, naRef* args)
{
    naRef v, h = argc > 0 ? args[0] : naNil();
    if(!naIsHash(h)) ARGERR();
    v = naNewVector(c);
    naHash_keys(v, h);
    return v;
}

static naRef f_append(naContext c, naRef me, int argc, naRef* args)
{
    int i;
    if(argc < 2 || !naIsVector(args[0])) ARGERR();
    for(i=1; i<argc; i++) naVec_append(args[0], args[i]);
    return args[0];
}

static naRef f_vec_remove(naContext c, naRef me, int argc, naRef* args)
{
    if (argc < 2 || !naIsVector(args[0])) ARGERR();
    naRef vec = args[0];
    naRef value = args[1];
    if (naIsNil(vec) || naIsNil(value)) ARGERR();

    int len = naVec_size(vec);
    int i;
    for (i = 0; i < len; i++) {
        if (naEqual(naVec_get(vec, i), value)) {
            naVec_remove(vec, i);
            --len;
        }
    }

    return args[0]; // return the vec to allow chaining
}


static naRef f_vec_removeat(naContext c, naRef me, int argc, naRef* args)
{
    if (argc < 2 || !naIsVector(args[0])) ARGERR();
    naRef vec = args[0];
    int index = (int)naNumValue(args[1]).num;
    if (naIsNil(vec)) ARGERR();

    int len = naVec_size(vec);
    if ((index < 0) || (index >= len)) ARGERR();

    return naVec_remove(vec, index);
}

static naRef f_pop(naContext c, naRef me, int argc, naRef* args)
{
    if(argc < 1 || !naIsVector(args[0])) ARGERR();
    return naVec_removelast(args[0]);
}

static naRef f_setsize(naContext c, naRef me, int argc, naRef* args)
{
    if(argc < 2 || !naIsVector(args[0])) ARGERR();
    naVec_setsize(c, args[0], (int)naNumValue(args[1]).num);
    return args[0];
}

static naRef f_subvec(naContext c, naRef me, int argc, naRef* args)
{
    int i;
    naRef nlen, result, v = args[0];
    int len = 0, start = (int)naNumValue(args[1]).num;
    if(argc < 2) return naNil();
    nlen = argc > 2 ? naNumValue(args[2]) : naNil();
    if(!naIsNil(nlen))
        len = (int)nlen.num;
    if(!naIsVector(v) || start < 0 || start > naVec_size(v) || len < 0)
        ARGERR();
    if(naIsNil(nlen) || len > naVec_size(v) - start)
        len = naVec_size(v) - start;
    result = naNewVector(c);
    naVec_setsize(c, result, len);
    for(i=0; i<len; i++)
        naVec_set(result, i, naVec_get(v, start + i));
    return result;
}

static naRef f_delete(naContext c, naRef me, int argc, naRef* args)
{
    if(argc < 2 || !naIsHash(args[0])) ARGERR();
    naHash_delete(args[0], args[1]);
    return args[0];
}

static naRef f_int(naContext c, naRef me, int argc, naRef* args)
{
    if(argc > 0) {
        naRef n = naNumValue(args[0]);
        if(naIsNil(n)) return n;
        if(n.num < 0) n.num = -floor(-n.num);
        else n.num = floor(n.num);
        return n;
    } else ARGERR();
    return naNil();
}

static naRef f_isint(naContext c, naRef me, int argc, naRef* args)
{
    if(argc > 0) {
        naRef n = naNumValue(args[0]);
        if(naIsNil(n)) return naNum(0);
        if(n.num < 0) n.num = -n.num;
        if(n.num == floor(n.num)) return naNum(1);
        else return naNum(0);
    } else ARGERR();
    return naNil();
}

static naRef f_num(naContext c, naRef me, int argc, naRef* args)
{
    return argc > 0 ? naNumValue(args[0]) : naNil();
}

static naRef f_isnum(naContext c, naRef me, int argc, naRef* args)
{
    if(argc > 0) {
        naRef n = naNumValue(args[0]);
        if(naIsNil(n)) return naNum(0);
        else return naNum(1);
    } else ARGERR();
    return naNil();
}

static naRef f_streq(naContext c, naRef me, int argc, naRef* args)
{
    return argc > 1 ? naNum(naStrEqual(args[0], args[1])) : naNil();
}

static naRef f_cmp(naContext c, naRef me, int argc, naRef* args)
{
    char *a, *b;
    int i, alen, blen;
    if(argc < 2 || !naIsString(args[0]) || !naIsString(args[1]))
        ARGERR();
    a = naStr_data(args[0]);
    alen = naStr_len(args[0]);
    b = naStr_data(args[1]);
    blen = naStr_len(args[1]);
    for(i=0; i<alen && i<blen; i++) {
        int diff = a[i] - b[i];
        if(diff) return naNum(diff < 0 ? -1 : 1);
    }
    return naNum(alen == blen ? 0 : (alen < blen ? -1 : 1));
}

static naRef f_str(naContext c, naRef me, int argc, naRef* args)
{
    if (argc < 1)
        ARGERR();
    
    return naStringValue(c, args[0]);
}

static naRef f_substr(naContext c, naRef me, int argc, naRef* args)
{
    int start, len, srclen;
    naRef src = argc > 0 ? args[0] : naNil();
    naRef startr = argc > 1 ? naNumValue(args[1]) : naNil();
    naRef lenr = argc > 2 ? naNumValue(args[2]) : naNil();
    if(!naIsString(src)) ARGERR();
    if(naIsNil(startr) || !naIsNum(startr)) ARGERR();
    if(!naIsNil(lenr) && !naIsNum(lenr)) ARGERR();
    srclen = naStr_len(src);
    start = (int)startr.num;
    len = naIsNum(lenr) ? (int)lenr.num : (srclen - start);
    if(start < 0) start += srclen;
    if(start < 0) start = len = 0;
    if(start >= srclen) start = len = 0;
    if(len < 0) len = 0;
    if(len > srclen - start) len = srclen - start;
    return naStr_substr(naNewString(c), src, start, len);
}

static naRef f_left(naContext c, naRef me, int argc, naRef* args)
{
    int len;
    naRef src = argc > 0 ? args[0] : naNil();
    naRef lenr = argc > 1 ? naNumValue(args[1]) : naNil();
    if(!naIsString(src)) ARGERR();
    if(!naIsNum(lenr)) ARGERR();
    len = (int)lenr.num;
    if(len < 0) len = 0;
    return naStr_substr(naNewString(c), src, 0, len);
}

static naRef f_right(naContext c, naRef me, int argc, naRef* args)
{
    int len, srclen;
    naRef src = argc > 0 ? args[0] : naNil();
    naRef lenr = argc > 1 ? naNumValue(args[1]) : naNil();
    if(!naIsString(src)) ARGERR();
    if(!naIsNum(lenr)) ARGERR();
    srclen = naStr_len(src);
    len = (int)lenr.num;
    if (len > srclen) len = srclen;
    if(len < 0) len = 0;
    return naStr_substr(naNewString(c), src, srclen - len, len);
}

static naRef f_chr(naContext c, naRef me, int argc, naRef* args)
{
    char chr[1];
    naRef cr = argc > 0 ? naNumValue(args[0]) : naNil();
    if(IS_NIL(cr)) ARGERR();
    chr[0] = (char)cr.num;
    return NEWSTR(c, chr, 1);
}

static naRef f_contains(naContext c, naRef me, int argc, naRef* args)
{
    naRef hashOrVec = argc > 0 ? args[0] : naNil();
    naRef key = argc > 1 ? args[1] : naNil();
    if (naIsNil(hashOrVec) || naIsNil(key)) ARGERR();

    if (naIsVector(hashOrVec)) {
        // very similar to vecindex below
        const int len = naVec_size(hashOrVec);
        int i;
        for (i = 0; i < len; i++) {
            if (naEqual(naVec_get(hashOrVec, i), key))
                return naNum(1);
        }
        return naNum(0); // not found in the vector
    } else if (naIsHash(hashOrVec)) {
        return naHash_get(hashOrVec, key, &key) ? naNum(1) : naNum(0);
    }

    return naNil();
}

static naRef f_vecindex(naContext c, naRef me, int argc, naRef* args)
{
    naRef vec = argc > 0 ? args[0] : naNil();
    naRef value = argc > 1 ? args[1] : naNil();
    if(naIsNil(vec) || naIsNil(value)) ARGERR();
    if(!naIsVector(vec)) return naNil();
    const int len = naVec_size(vec);
    int i;
    for(i=0; i<len; i++) {
        if (naEqual(naVec_get(vec, i), value))
            return naNum(i);
    }
    
    return naNil();
}

static naRef f_typeof(naContext c, naRef me, int argc, naRef* args)
{
    naRef r = argc > 0 ? args[0] : naNil();
    char* t = "unknown";
    if(naIsNil(r)) t = "nil";
    else if(naIsNum(r)) t = "scalar";
    else if(naIsString(r)) t = "scalar";
    else if(naIsVector(r)) t = "vector";
    else if(naIsHash(r)) t = "hash";
    else if(naIsFunc(r)) t = "func";
    else if(naIsGhost(r)) t = "ghost";
    return NEWCSTR(c, t);
}

static naRef f_isscalar(naContext c, naRef me, int argc, naRef* args)
{
    naRef r = argc > 0 ? args[0] : naNil();
    if(naIsString(r) || naIsNum(r)) return naNum(1);
    else return naNum(0);
}

static naRef f_isstr(naContext c, naRef me, int argc, naRef* args)
{
    naRef r = argc > 0 ? args[0] : naNil();
    if(naIsString(r)) return naNum(1);
    else return naNum(0);
}

static naRef f_isvec(naContext c, naRef me, int argc, naRef* args)
{
    naRef r = argc > 0 ? args[0] : naNil();
    if(naIsVector(r)) return naNum(1);
    else return naNum(0);
}

static naRef f_ishash(naContext c, naRef me, int argc, naRef* args)
{
    naRef r = argc > 0 ? args[0] : naNil();
    if(naIsHash(r)) return naNum(1);
    else return naNum(0);
}

static naRef f_isfunc(naContext c, naRef me, int argc, naRef* args)
{
    naRef r = argc > 0 ? args[0] : naNil();
    if(naIsFunc(r)) return naNum(1);
    else return naNum(0);
}

static naRef f_isghost(naContext c, naRef me, int argc, naRef* args)
{
    naRef r = argc > 0 ? args[0] : naNil();
    if(naIsGhost(r)) return naNum(1);
    else return naNum(0);
}

static naRef f_ghosttype(naContext c, naRef me, int argc, naRef* args)
{
    naRef g = argc > 0 ? args[0] : naNil();
    if(!naIsGhost(g)) return naNil();
    if(naGhost_type(g)->name) {
        return NEWCSTR(c, (char*)naGhost_type(g)->name);
    } else {
        char buf[128];
        snprintf(buf, 128, "%p", naGhost_type(g));
        return NEWCSTR(c, buf);
    }
}

static naRef f_compile(naContext c, naRef me, int argc, naRef* args)
{
    int errLine;
    naRef script, code, fname;
    script = argc > 0 ? args[0] : naNil();
    fname = argc > 1 ? args[1] : NEWCSTR(c, "<compile>");
    if(!naIsString(script) || !naIsString(fname)) return naNil();
    code = naParseCode(c, fname, 1,
                       naStr_data(script), naStr_len(script), &errLine);
    if(naIsNil(code)) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Parse error: %s at line %d",
                 naGetError(c), errLine);
        c->dieArg = NEWCSTR(c, buf);
        naRuntimeError(c, "__die__");
    }
    return naBindToContext(c, code);
}

// FIXME: need a place to save the current IP when we get an error so
// that it can be reset if we get a die()/naRethrowError() situation
// later.  Right now, the IP on the stack trace is the line of the
// die() call, when it should be this one...
//
// FIXME: don't use naCall at all here, we don't need it.  Fix up the
// context stack to tail call the function directly.  There's no need
// for f_call() to live on the C stack at all.
static naRef f_call(naContext c, naRef me, int argc, naRef* args)
{
    naContext subc;
    naRef callargs, callme, callns, result;
    struct VecRec* vr;
    callargs = argc > 1 ? args[1] : naNil();
    callme = argc > 2 ? args[2] : naNil(); // Might be nil, that's OK
    callns = argc > 3 ? args[3] : naNil(); // ditto
    if(!IS_HASH(callme) && !IS_GHOST(callme)) callme = naNil();
    if(!IS_HASH(callns)) callns = naNil();
    if(argc==0 || !IS_FUNC(args[0]) || (!IS_NIL(callargs) && !IS_VEC(callargs)))
        ARGERR();

    subc = naSubContext(c);
    vr = IS_NIL(callargs) ? 0 : PTR(callargs).vec->rec;
    result = naCall(subc, args[0], vr ? vr->size : 0, vr ? vr->array : 0,
                    callme, callns);
    if(!naGetError(subc)) {
        naFreeContext(subc);
        return result;
    }

    // Error handling. Note that we don't free the subcontext after an
    // error, in case the user re-throws the same error or calls
    // naContinue()
    if(argc <= 2 || !IS_VEC(args[argc-1])) {
        naRethrowError(subc);
    } else {
        int i, sd;
        naRef errv = args[argc-1];
        if(!IS_NIL(subc->dieArg)) naVec_append(errv, subc->dieArg);
        else naVec_append(errv, NEWCSTR(subc, naGetError(subc)));
        sd = naStackDepth(subc);
        for(i=0; i<sd; i++) {
            naVec_append(errv, naGetSourceFile(subc, i));
            naVec_append(errv, naNum(naGetLine(subc, i)));
        }
    }
    return naNil();
}

static naRef f_die(naContext c, naRef me, int argc, naRef* args)
{
    naRef darg = argc > 0 ? args[0] : naNil();
    if(!naIsNil(darg) && c->callChild && IDENTICAL(c->callChild->dieArg, darg))
        naRethrowError(c->callChild);
    c->dieArg = darg;
    naRuntimeError(c, "__die__");
    return naNil(); // never executes
}

char* dosprintf(char* f, ...)
{
    char* buf;

    va_list va;
    va_start(va, f);
   
    int len = vsnprintf(0, 0, f, va);
    va_end(va);

    if (len <= 0) {
        buf = (char *) naAlloc(2);
        *buf = 0;
    }
    else {
        len++;// allow for terminating null
        buf = (char *) naAlloc(len);

        va_list va;
        va_start(va, f);

        len = vsnprintf(buf, len, f, va);
        va_end(va);
    }

    return buf;
}

// Inspects a printf format string f, and finds the next "%..." format
// specifier.  Stores the start of the specifier in out, the length in
// len, and the type in type.  Returns a pointer to the remainder of
// the format string, or 0 if no format string was found.  Recognizes
// all of ANSI C's syntax except for the "length modifier" feature.
// Note: this does not validate the format character returned in
// "type". That is the caller's job.
static char* nextFormat(naContext c, char* f, char** out, int* len, char* type)
{
    // Skip to the start of the format string
    while(*f && *f != '%') f++;
    if(!*f) return 0;
    *out = f++;

    while(*f && (*f=='-' || *f=='+' || *f==' ' || *f=='0' || *f=='#')) f++;

    // Test for duplicate flags.  This is pure pedantry and could
    // be removed on all known platforms, but just to be safe...
    {   char *p1, *p2;
        for(p1 = *out + 1; p1 < f; p1++)
            for(p2 = p1+1; p2 < f; p2++)
                if(*p1 == *p2)
                    naRuntimeError(c, "duplicate flag in format string"); }

    while(*f && *f >= '0' && *f <= '9') f++;
    if(*f && *f == '.') f++;
    while(*f && *f >= '0' && *f <= '9') f++;
    if(!*f) naRuntimeError(c, "invalid format string");

    *type = *f++;
    *len = f - *out;
    return f;
}

#define ERR(m) naRuntimeError(c, m)
#define APPEND(r) result = naStr_concat(naNewString(c), result, r)
static naRef f_sprintf(naContext c, naRef me, int argc, naRef* args)
{
    char t, nultmp, *fstr, *next, *fout=0, *s;
    int flen, argn=1;
    naRef format, arg, result = naNewString(c);

    if(argc < 1) ERR("not enough arguments to sprintf()");
    format = naStringValue(c, argc > 0 ? args[0] : naNil());
    if(naIsNil(format)) ERR("bad format string in sprintf()");
    s = naStr_data(format);
                               
    while((next = nextFormat(c, s, &fstr, &flen, &t))) {
        APPEND(NEWSTR(c, s, fstr-s)); // stuff before the format string
        if(flen == 2 && fstr[1] == '%') {
            APPEND(NEWSTR(c, "%", 1));
            s = next;
            continue;
        }
        if(argn >= argc) ERR("not enough arguments to sprintf()");
        arg = args[argn++];
        nultmp = fstr[flen]; // sneaky nul termination...
        fstr[flen] = 0;
        if(t == 's') {
            arg = naStringValue(c, arg);
            if(naIsNil(arg)) fout = dosprintf(fstr, "nil");
            else             fout = dosprintf(fstr, naStr_data(arg));
        } else {
            arg = naNumValue(arg);
            if(naIsNil(arg))
                fout = dosprintf("nil");
            else if(t=='d' || t=='i' || t=='c')
                fout = dosprintf(fstr, (int)naNumValue(arg).num);
            else if(t=='o' || t=='u' || t=='x' || t=='X')
                fout = dosprintf(fstr, (unsigned int)naNumValue(arg).num);
            else if(t=='e' || t=='E' || t=='f' || t=='F' || t=='g' || t=='G')
                fout = dosprintf(fstr, naNumValue(arg).num);
            else
                ERR("invalid sprintf format type");
        }
        fstr[flen] = nultmp;
        APPEND(NEWSTR(c, fout, strlen(fout)));
        naFree(fout);
        s = next;
    }
    APPEND(NEWSTR(c, s, strlen(s)));
    return result;
}

// FIXME: needs to honor subcontext list
static naRef f_caller(naContext c, naRef me, int argc, naRef* args)
{
    int fidx;
    struct Frame* frame;
    naRef result, fr = argc ? naNumValue(args[0]) : naNum(1);
    if(IS_NIL(fr)) ARGERR();
    fidx = (int)fr.num;
    if(fidx > c->fTop - 1) return naNil();
    frame = &c->fStack[c->fTop - 1 - fidx];
    result = naNewVector(c);
    naVec_append(result, frame->locals);
    naVec_append(result, frame->func);
    naVec_append(result, PTR(PTR(frame->func).func->code).code->srcFile);
    naVec_append(result, naNum(naGetLine(c, fidx)));
    return result;
}

static naRef f_closure(naContext c, naRef me, int argc, naRef* args)
{
    int i;
    struct naFunc* f;
    naRef func = argc > 0 ? args[0] : naNil();
    naRef idx = argc > 1 ? naNumValue(args[1]) : naNum(0);
    if(!IS_FUNC(func) || IS_NIL(idx)) ARGERR();
    i = (int)idx.num;
    f = PTR(func).func;
    while(i > 0 && f) { i--; f = PTR(f->next).func; }
    if(!f) return naNil();
    return f->namespace;
}

static int match(unsigned char* a, unsigned char* b, int l)
{
    int i;
    for(i=0; i<l; i++) if(a[i] != b[i]) return 0;
    return 1;
}

static int find(unsigned char* a, int al, unsigned char* s, int sl, int start)
{
    int i;
    if(al == 0) return 0;
    for(i=start; i<sl-al+1; i++) if(match(a, s+i, al)) return i;
    return -1;
}

static naRef f_find(naContext c, naRef me, int argc, naRef* args)
{
    int start = 0;
    if(argc < 2 || !IS_STR(args[0]) || !IS_STR(args[1])) ARGERR();
    if(argc > 2) start = (int)(naNumValue(args[2]).num);
    return naNum(find((void*)naStr_data(args[0]), naStr_len(args[0]),
                      (void*)naStr_data(args[1]), naStr_len(args[1]),
                      start));
}

static naRef f_split(naContext c, naRef me, int argc, naRef* args)
{
    int sl, dl, i;
    char *s, *d, *s0;
    naRef result;
    if(argc < 2 || !IS_STR(args[0]) || !IS_STR(args[1])) ARGERR();
    d = naStr_data(args[0]); dl = naStr_len(args[0]);
    s = naStr_data(args[1]); sl = naStr_len(args[1]);
    result = naNewVector(c);
    if(dl == 0) { // special case zero-length delimiter
        for(i=0; i<sl; i++) naVec_append(result, NEWSTR(c, s+i, 1));
        return result;
    }
    s0 = s;
    for(i=0; i <= sl-dl; i++) {
        if(match((unsigned char*)(s+i), (unsigned char*)d, dl)) {
            naVec_append(result, NEWSTR(c, s0, s+i-s0));
            s0 = s + i + dl;
            i += dl - 1;
        }
    }
    if(s0 - s <= sl) naVec_append(result, NEWSTR(c, s0, s+sl-s0));
    return result;
}

// This is a comparatively weak RNG, based on the C library's rand()
// function, which is usually not threadsafe and often of limited
// precision.  The 5x loop guarantees that we get a full double worth
// of precision even for 15 bit (Win32...) rand() implementations.
static naRef f_rand(naContext c, naRef me, int argc, naRef* args)
{
    int i;
    double r = 0;
    if(argc) {
        if(!IS_NUM(args[0])) naRuntimeError(c, "rand() seed not number");
        srand((unsigned int)args[0].num);
        return naNil();
    }
    for(i=0; i<5; i++) r = (r + rand()) * (1.0/(RAND_MAX+1.0));
    return naNum(r);
}

static naRef f_bind(naContext c, naRef me, int argc, naRef* args)
{
    naRef func = argc > 0 ? args[0] : naNil();
    naRef hash = argc > 1 ? args[1] : naNewHash(c);
    naRef next = argc > 2 ? args[2] : naNil();
    if(!IS_FUNC(func) || (!IS_NIL(next) && !IS_FUNC(next)) || !IS_HASH(hash))
        ARGERR();
    func = naNewFunc(c, PTR(func).func->code);
    PTR(func).func->namespace = hash;
    PTR(func).func->next = next;
    return func;
}

/* We use the "SortRec" gadget for two reasons: first, because ANSI
 * qsort() doesn't give us a mechanism for passing a "context" pointer
 * to the comparison routine we have to store one in every sorted
 * record.  Second, using an index into the original vector here
 * allows us to make the sort stable in the event of a zero returned
 * from the Nasal comparison function. */
struct SortData { naContext ctx, subc; struct SortRec* recs;
                  naRef* elems; int n; naRef fn; };
struct SortRec { struct SortData* sd; int i; };

static int sortcmp(struct SortRec* a, struct SortRec* b)
{
    struct SortData* sd = a->sd;
    naRef args[2], d;
    args[0] = sd->elems[a->i];
    args[1] = sd->elems[b->i];
    d = naCall(sd->subc, sd->fn, 2, args, naNil(), naNil());
    if(naGetError(sd->subc)) {
        naFree(sd->recs);
        naRethrowError(sd->subc);
    } else if(!naIsNum(d = naNumValue(d))) {
        naFree(sd->recs);
        naRuntimeError(sd->ctx, "sort() comparison returned non-number");
    }
    return (d.num > 0) ? 1 : ((d.num < 0) ? -1 : (a->i - b->i));
}

static naRef f_sort(naContext c, naRef me, int argc, naRef* args)
{
    int i;
    struct SortData sd;
    naRef out;
    if(argc != 2 || !naIsVector(args[0]) || !naIsFunc(args[1]))
        naRuntimeError(c, "bad/missing argument to sort()");
    sd.subc = naSubContext(c);
    if(!PTR(args[0]).vec->rec) return naNewVector(c);
    sd.elems = PTR(args[0]).vec->rec->array;
    sd.n = PTR(args[0]).vec->rec->size;
    sd.fn = args[1];
    sd.recs = naAlloc(sizeof(struct SortRec) * sd.n);
    for(i=0; i<sd.n; i++) {
        sd.recs[i].sd = &sd;
        sd.recs[i].i = i;
    }
    qsort(sd.recs, sd.n, sizeof(sd.recs[0]),
          (int(*)(const void*,const void*))sortcmp);
    out = naNewVector(c);
    naVec_setsize(c, out, sd.n);
    for(i=0; i<sd.n; i++)
        PTR(out).vec->rec->array[i] = sd.elems[sd.recs[i].i];
    naFree(sd.recs);
    naFreeContext(sd.subc);
    return out;
}

static naRef f_id(naContext c, naRef me, int argc, naRef* args)
{
    char *t = "unk", buf[64];
    if(argc != 1 || !IS_REF(args[0]))
        naRuntimeError(c, "bad/missing argument to id()");
    if     (IS_STR(args[0]))   t = "str";
    else if(IS_VEC(args[0]))   t = "vec";
    else if(IS_HASH(args[0]))  t = "hash";
    else if(IS_CODE(args[0]))  t = "code";
    else if(IS_FUNC(args[0]))  t = "func";
    else if(IS_CCODE(args[0])) t = "ccode";
    else if(IS_GHOST(args[0])) {
        naGhostType *gt = PTR(args[0]).ghost->gtype;
        t = gt->name ? (char*)gt->name : "ghost";
    }
    snprintf(buf, sizeof(buf), "%s:%p", (char*)t, (void*)PTR(args[0]).obj);
    return NEWCSTR(c, buf);
}

// Python-like range (a vector filled with numbers from start to stop, spaced by step)
// Synopsis:
//    range([start, ]stop[, step])
//        start=0:    Optional. An integer number specifying at which position to start.
//        stop:        Required. A number specifying at which position to stop (not included)
//        step=1:        Optional. An integer number greater 0 specifying the incrementation.
//
// Examples:
//        var r = range(5);
//         debug.dump(r)
//    prints [0, 1, 2, 3, 4]
//
//        var r = range(2, 5);
//         debug.dump(r)
//    prints [2, 3, 4]
//
//        var r = range(0, 10, 2);
//         debug.dump(r)
//    prints [0, 2, 4, 6, 8]
//
//        var r = range(0, 0);
//        debug.dump(r)
//    prints []
//
//        var r = range(0, 1);
//        debug.dump(r)
//    prints [0]
//
//        var r = range(0, 0.1);
//        debug.dump(r)
//    prints [0]
static naRef f_range(naContext c, naRef me, int argc, naRef* args)
{
    int start = 0;
    int step = 1;
    int end = 0;
    naRef out;

    if ((argc < 1) || (argc > 3) || !naIsNum(args[0]))
        naRuntimeError(c, "Bad/missing argument to range()");
    
    if (argc > 1) {
        if (!naIsNum(args[1])) {
            naRuntimeError(c, "Invalid argument to range()");
        }

        start = (int) args[0].num;
        end = (int) args[1].num;

        if (argc > 2) {
            if (!naIsNum(args[2])) {
                naRuntimeError(c, "Invalid argument to range()");
            }

            step = (int) args[2].num;
        }
    } else {
        // single arg mode
        end = (int) args[0].num;
    }

    out = naNewVector(c);
    for (int i=start; i<end; i+=step) {
        naVec_append(out, naNum(i));
    }
    return out;
}

static naCFuncItem funcs[] = {
    {"size", f_size},
    {"keys", f_keys},
    {"append", f_append},
    {"pop", f_pop},
    {"setsize", f_setsize},
    {"subvec", f_subvec},
    {"vecindex", f_vecindex},
    {"remove", f_vec_remove},
    {"removeat", f_vec_removeat},
    {"delete", f_delete},
    {"int", f_int},
    {"num", f_num},
    {"str", f_str},
    {"streq", f_streq},
    {"cmp", f_cmp},
    {"substr", f_substr},
    {"left", f_left},
    {"right", f_right},
    {"chr", f_chr},
    {"contains", f_contains},
    {"typeof", f_typeof},
    {"ghosttype", f_ghosttype},
    {"compile", f_compile},
    {"call", f_call},
    {"die", f_die},
    {"sprintf", f_sprintf},
    {"caller", f_caller},
    {"closure", f_closure},
    {"find", f_find},
    {"split", f_split},
    {"rand", f_rand},
    {"bind", f_bind},
    {"sort", f_sort},
    {"id", f_id},
    {"isscalar", f_isscalar},
    {"isint", f_isint},
    {"isnum", f_isnum},
    {"isghost", f_isghost},
    {"isstr", f_isstr},
    {"isvec", f_isvec},
    {"ishash", f_ishash},
    {"isfunc", f_isfunc},
    {"range", f_range},
    {0}};

naRef naInit_std(naContext c)
{
    return naGenLib(c, funcs);
}
