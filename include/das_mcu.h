#pragma once

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <new>
#include <zephyr/sys/printk.h>

inline void *operator new(size_t, void *p) noexcept { return p; }
inline void operator delete(void *, void *) noexcept {}

#define DAS_API
#define __forceinline inline
#define DAS_ASSERT(x) ((void)0)
#define DAS_COMMENT(...)
#define DAS_BIND_ENUM_CAST(enum_name)
#define DAS_BIND_ENUM_CAST_98(enum_name)
#define DAS_BIND_ENUM_CAST_98_IN_NAMESPACE(enum_name, stripped_enum_name)

namespace das {

template <typename T>
T &&das_move(T &v) { return static_cast<T &&>(v); }

struct string {
	const char *s = "";
	string() = default;
	string(const char *p) : s(p ? p : "") {}
	const char *c_str() const { return s; }
};

template <typename A, typename B>
struct pair {
	A first{};
	B second{};
	pair() = default;
	pair(const A &a, const B &b) : first(a), second(b) {}
};

template <typename T>
struct vector {
	T *d = nullptr;
	int n = 0, c = 0;
	~vector() { delete[] d; }
	template <typename... Args>
	void emplace_back(Args &&...args)
	{
		if (n == c) {
			int nc = c ? c * 2 : 4;
			T *nd = new T[nc];
			for (int i = 0; i < n; ++i) nd[i] = d[i];
			delete[] d;
			d = nd;
			c = nc;
		}
		d[n++] = T(static_cast<Args &&>(args)...);
	}
	T *begin() { return d; }
	T *end() { return d + n; }
};

template <typename T>
struct shared_ptr {
	T *p = nullptr;
	shared_ptr() = default;
	explicit shared_ptr(T *x) : p(x) {}
	T *operator->() const { return p; }
	T &operator*() const { return *p; }
};

template <typename T>
shared_ptr<T> make_shared() { return shared_ptr<T>(new T()); }

struct recursive_mutex {};

template <typename K, typename V>
struct unordered_map {
	struct Node {
		K first{};
		V second{};
		bool used = false;
	};
	mutable Node nodes[64]{};
	struct iterator {
		Node *p = nullptr;
		Node *operator->() const { return p; }
		bool operator!=(iterator o) const { return p != o.p; }
		bool operator==(iterator o) const { return p == o.p; }
	};
		iterator end() const { return {const_cast<Node *>(nodes + 64)}; }
	iterator find(const K &k) const
	{
		for (auto &x : nodes)
			if (x.used && x.first == k) return {&x};
		return end();
	}
	template <typename KK, typename VV>
	void emplace(KK &&k, VV &&v)
	{
		for (auto &x : nodes) {
			if (!x.used) {
				x.used = true;
				x.first = static_cast<KK &&>(k);
				x.second = static_cast<VV &&>(v);
				return;
			}
		}
	}
	V &operator[](const K &k)
	{
		auto it = find(k);
		if (it != end()) return it->second;
		for (auto &x : nodes) {
			if (!x.used) {
				x.used = true;
				x.first = k;
				return x.second;
			}
		}
		return nodes[0].second;
	}
	void clear()
	{
		for (auto &x : nodes) x = Node{};
	}
};

template <typename K, typename V>
using das_hash_map = unordered_map<K, V>;

inline uint64_t hash_blockz64(const uint8_t *block)
{
	uint64_t h = UINT64_C(14695981039346656037), p = UINT64_C(1099511628211);
	if (!block) return h;
	while (true) {
		uint64_t v = block[0];
		if (!v) break;
		v |= uint64_t(block[1]) << 8;
		h = (h ^ v) * p;
		if (v < 0x100) break;
		block += 2;
	}
	return h <= 1 ? UINT64_C(1099511628211) : h;
}

struct Variable {
	static uint64_t getMNHash(const string &n)
	{
		return hash_blockz64(reinterpret_cast<const uint8_t *>(n.c_str()));
	}
};

struct vec4f {
	union {
		float f[4];
		int32_t i[4];
		uint32_t u[4];
		uint64_t u64[2];
		void *p;
	};
};

inline vec4f v_zero()
{
	vec4f z;
	memset(&z, 0, sizeof(z));
	return z;
}

template <typename TT>
struct TypeSize {
	enum { size = int(sizeof(TT)) };
};
template <>
struct TypeSize<void> {
	enum { size = 0 };
};

enum Type : int32_t {
	none,
	autoinfer,
	alias,
	option,
	typeDecl,
	typeMacro,
	fakeContext,
	fakeLineInfo,
	anyArgument,
	tVoid,
	tBool,
	tInt8,
	tUInt8,
	tInt16,
	tUInt16,
	tInt64,
	tUInt64,
	tInt,
	tInt2,
	tInt3,
	tInt4,
	tUInt,
	tUInt2,
	tUInt3,
	tUInt4,
	tFloat,
	tFloat2,
	tFloat3,
	tFloat4,
	tDouble,
	tRange,
	tURange,
	tRange64,
	tURange64,
	tString,
	tStructure,
	tHandle,
	tEnumeration,
	tEnumeration8,
	tEnumeration16,
	tEnumeration64,
	tBitfield,
	tBitfield8,
	tBitfield16,
	tBitfield64,
	tPointer,
	tFunction,
	tLambda,
	tIterator,
	tArray,
	tTable,
	tBlock,
	tTuple,
	tVariant
};

struct Annotation;
struct StructInfo;
struct EnumInfo;
struct LocalVariableInfo;

struct AnnotationInfo {
	const char *name = nullptr;
	const char *module_name = nullptr;
	void *arguments = nullptr;
	uint32_t count = 0;
	mutable Annotation *resolved = nullptr;
	AnnotationInfo() = default;
	AnnotationInfo(const char *n, const char *m, void *a, uint32_t c)
		: name(n), module_name(m), arguments(a), count(c) {}
};

struct TypeInfo {
	union {
		StructInfo *structType;
		EnumInfo *enumType;
		AnnotationInfo *annotation_info;
	};
	TypeInfo *firstType = nullptr, *secondType = nullptr, **argTypes = nullptr;
	const char **argNames = nullptr;
	uint32_t *dim = nullptr;
	uint64_t hash = 0;
	Type type = Type::none;
	uint32_t flags = 0, size = 0, argCount = 0, dimSize = 0;
	TypeInfo() = default;
	TypeInfo(Type t, StructInfo *st, EnumInfo *et, AnnotationInfo *an, TypeInfo *a, TypeInfo *b,
		 TypeInfo **at, const char **anm, uint32_t ac, uint32_t ds, uint32_t *d, uint32_t fl,
		 uint32_t sz, uint64_t h)
		: firstType(a), secondType(b), argTypes(at), argNames(anm), dim(d), hash(h), type(t),
		  flags(fl), size(sz), argCount(ac), dimSize(ds)
	{
		if (st) structType = st;
		else if (et) enumType = et;
		else annotation_info = an;
	}
	void resolveAnnotation() const {}
};

struct VarInfo : TypeInfo {
	vec4f value{};
	const char *name = nullptr;
	void *annotation_arguments = nullptr;
	uint32_t annotation_argument_count = 0;
	uint32_t offset = 0, nextGcField = 0;
	VarInfo() = default;
	VarInfo(Type t, StructInfo *st, EnumInfo *et, AnnotationInfo *an, TypeInfo *a, TypeInfo *b,
		TypeInfo **at, const char **anm, uint32_t ac, uint32_t ds, uint32_t *d, uint32_t fl,
		uint32_t sz, uint64_t h, const char *n, uint32_t off, uint32_t next_gc)
		: TypeInfo(t, st, et, an, a, b, at, anm, ac, ds, d, fl, sz, h), name(n), offset(off),
		  nextGcField(next_gc)
	{
	}
};

struct StructInfo {
	const char *name = nullptr;
	const char *module_name = nullptr;
	VarInfo **fields = nullptr;
	AnnotationInfo *annotations = nullptr;
	uint64_t hash = 0;
	uint64_t init_mnh = 0;
	uint32_t flags = 0;
	uint32_t count = 0;
	uint32_t size = 0;
	uint32_t firstGcField = 0;
	uint32_t annotation_count = 0;
	StructInfo() = default;
	StructInfo(const char *_name, const char *_module_name, uint32_t _flags, VarInfo **_fields,
		   uint32_t _count, uint32_t _size, uint64_t _init_mnh, AnnotationInfo *_annotations,
		   uint32_t _annotation_count, uint64_t _hash, uint32_t _firstGcField)
		: name(_name), module_name(_module_name), fields(_fields), annotations(_annotations),
		  hash(_hash), init_mnh(_init_mnh), flags(_flags), count(_count), size(_size),
		  firstGcField(_firstGcField), annotation_count(_annotation_count)
	{
	}
};

struct FuncInfo {
	const char *name = nullptr, *cppName = nullptr;
	VarInfo **fields = nullptr;
	uint32_t count = 0, stackSize = 0;
	TypeInfo *result = nullptr;
	LocalVariableInfo **locals = nullptr;
	uint32_t localCount = 0;
	uint64_t hash = 0;
	uint32_t flags = 0;
	VarInfo **globals = nullptr;
	uint32_t globalCount = 0;
	AnnotationInfo *annotations = nullptr;
	uint32_t annotation_count = 0, spaceId = 0;
	FuncInfo() = default;
	FuncInfo(const char *n, const char *c, VarInfo **f, uint32_t cnt, uint32_t ss, TypeInfo *r,
		 LocalVariableInfo **loc, uint32_t lc, uint64_t h, uint32_t fl, AnnotationInfo *ann = nullptr,
		 uint32_t ac = 0)
		: name(n), cppName(c), fields(f), count(cnt), stackSize(ss), result(r), locals(loc),
		  localCount(lc), hash(h), flags(fl), annotations(ann), annotation_count(ac) {}
};

struct LineInfo {
	static LineInfo g_LineInfoNULL;
};
struct LineInfoArg : LineInfo {};
inline LineInfo LineInfo::g_LineInfoNULL{};
class Context;
struct SimNode;

struct GlobalVariable {
	char *name = nullptr;
	VarInfo *debugInfo = nullptr;
	SimNode *init = nullptr;
	uint64_t mangledNameHash = 0;
	uint32_t size = 0, offset = 0;
	union {
		struct { bool shared : 1; };
		uint32_t flags = 0;
	};
};

struct SimFunction {
	char *name = nullptr, *mangledName = nullptr;
	SimNode *code = nullptr;
	FuncInfo *debugInfo = nullptr;
	uint64_t mangledNameHash = 0;
	void *aotFunction = nullptr, *jitFunction = nullptr;
	uint32_t stackSize = 0;
	union {
		uint32_t flags = 0;
		struct {
			bool aot : 1, fastcall : 1, builtin : 1, jit : 1, unsafe : 1, cmres : 1, pinvoke : 1;
		};
	};
};

struct SimNode {
	LineInfo debugInfo;
	explicit SimNode(const LineInfo &at) : debugInfo(at) {}
	virtual ~SimNode() = default;
	virtual vec4f eval(Context &) { return v_zero(); }
};

struct SimNode_WithErrorMessage : SimNode {
	SimNode_WithErrorMessage(const LineInfo &at, const char *) : SimNode(at) {}
};
struct SimNode_CallBase : SimNode_WithErrorMessage {
	SimNode_CallBase(const LineInfo &at, const char *msg) : SimNode_WithErrorMessage(at, msg) {}
	void *aotFunction = nullptr;
	TypeInfo **types = nullptr;
	int32_t nArguments = 0;
};

struct Func {
	SimFunction *PTR = nullptr;
	Func() = default;
	Func(SimFunction *p) : PTR(p) {}
	explicit operator bool() const { return PTR != nullptr; }
};

struct Lambda {
	char *capture = nullptr;
	Lambda() = default;
	Lambda(void *ptr) : capture((char *)ptr) {}
	operator void *() const { return capture; }
	bool operator==(void *ptr) const { return capture == ptr; }
	bool operator!=(void *ptr) const { return capture != ptr; }
};

struct Array {
	char *data = nullptr;
	uint64_t size = 0;
	uint64_t capacity = 0;
	uint32_t magic = 0;
	uint32_t lock = 0;
	uint32_t flags = 0;
};

struct range {
	int32_t from = 0;
	int32_t to = 0;
	range() = default;
	explicit range(int32_t t) : from(0), to(t) {}
	range(int32_t f, int32_t t) : from(f), to(t) {}
};

inline range mk_range(int32_t i) { return range(i); }

struct NodeAllocator {
	char buf[16384]{};
	size_t used = 0;
	char *allocate(uint32_t n)
	{
		n = (n + 15u) & ~15u;
		if (used + n > sizeof(buf)) return nullptr;
		char *p = buf + used;
		used += n;
		return p;
	}
	char *allocateName(const string &s)
	{
		auto n = strlen(s.c_str()) + 1;
		char *p = allocate(uint32_t(n));
		if (p) memcpy(p, s.c_str(), n);
		return p;
	}
	template <typename T, typename... Args>
	T *makeNode(Args &&...args)
	{
		return new (allocate(uint32_t(sizeof(T)))) T(static_cast<Args &&>(args)...);
	}
};

struct CodeOfPolicies {
	bool debugger = false, persistent_heap = false, track_allocations = false;
	uint32_t heap_size_hint = 0, string_heap_size_hint = 0;
};

struct AnnotationArgumentList {};

struct FunctionInfo {
	FunctionInfo(string n, string mn, uint64_t m, uint64_t a, uint32_t ss, bool u, bool f, bool b,
		     bool pr, bool rr, bool pi)
		: name(das_move(n)), mangledName(das_move(mn)), mnh(m), aotHash(a), stackSize(ss),
		  unsafeOperation(u), fastCall(f), builtin(b), promoted(pr), res_ref(rr), pinvoke(pi) {}
	string name, mangledName;
	uint64_t mnh, aotHash;
	uint32_t stackSize;
	bool unsafeOperation, fastCall, builtin, promoted, res_ref, pinvoke;
};

struct GlobalVarInfo {
	GlobalVarInfo(string n, const string &mn, uint32_t sz, bool sh)
		: name(das_move(n)), mangledNameHash(Variable::getMNHash(mn)), typeSize(sz), globalShared(sh) {}
	string name;
	uint64_t mangledNameHash;
	uint32_t typeSize;
	bool globalShared;
};

struct SizeDiff {
	uint64_t sharedSizeDiff, globalsSizeDiff;
};

class Context {
public:
	Context(uint32_t = 0, bool = false) { code = make_shared<NodeAllocator>(); }
	virtual ~Context()
	{
		delete[] globals;
		delete contextMutex;
		delete tabMnLookup.p;
		delete tabGMnLookup.p;
		delete tabAdLookup.p;
		delete code.p;
	}
	void setup(int nvars, uint32_t, CodeOfPolicies, AnnotationArgumentList)
	{
		totalVariables = nvars;
		globalVariables = (GlobalVariable *)code->allocate(uint32_t(nvars * sizeof(GlobalVariable)));
	}
	void allocateGlobalsAndShared()
	{
		globals = globalsSize ? new char[globalsSize] : nullptr;
		if (globals) memset(globals, 0, globalsSize);
	}
	void runInitScript() {}
	void updateSharedGlobalSize(uint64_t, uint64_t g) { globalsSize += g; }
	uint64_t getGlobalSize() const { return globalsSize; }
	uint64_t getSharedSize() const { return 0; }
	uint32_t globalOffsetByMangledName(uint64_t mnh)
	{
		auto it = tabGMnLookup->find(mnh);
		return it != tabGMnLookup->end() ? it->second : 0;
	}
	SimFunction *fnByMangledName(uint64_t mnh)
	{
		if (!mnh) return nullptr;
		auto it = tabMnLookup->find(mnh);
		return it != tabMnLookup->end() ? it->second : nullptr;
	}
	vec4f *abiArguments() { return abiArg; }

	char *allocate(uint32_t n)
	{
		if (!n) return nullptr;
		return new char[n];
	}
	void free(char *p, uint32_t) { delete[] p; }

	void throw_error(const char *msg)
	{
		printk("das error: %s\n", msg ? msg : "");
		for (;;) {
		}
	}
	void throw_error_at(const LineInfo *, const char *fmt, ...)
	{
		char buf[128];
		va_list ap;
		va_start(ap, fmt);
		vsnprintk(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		throw_error(buf);
	}
	void throw_error_ex(const char *fmt, ...)
	{
		char buf[128];
		va_list ap;
		va_start(ap, fmt);
		vsnprintk(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		throw_error(buf);
	}

	vec4f abiArgBuf[8]{};
	vec4f *abiArg = abiArgBuf;
	void *abiCMRES = nullptr;
	shared_ptr<NodeAllocator> code;
	char *globals = nullptr;
	GlobalVariable *globalVariables = nullptr;
	SimFunction *functions = nullptr;
	int totalVariables = 0, totalFunctions = 0;
	uint64_t globalsSize = 0;
	recursive_mutex *contextMutex = nullptr;
	shared_ptr<das_hash_map<uint64_t, SimFunction *>> tabMnLookup;
	shared_ptr<das_hash_map<uint64_t, uint32_t>> tabGMnLookup;
	shared_ptr<das_hash_map<uint64_t, uint64_t>> tabAdLookup;
};

inline void builtin_print(char *text, Context *, LineInfoArg *)
{
	printk("%s", text ? text : "");
}

inline void das_assert(bool cond, Context *ctx)
{
	if (!cond) ctx->throw_error("assert failed");
}

template <typename TT>
struct TArray : Array {
	TArray() = default;
	TArray(TArray &arr) { moveA(arr); }
	TArray(TArray &&arr) { moveA(arr); }
	TArray &operator=(TArray &arr)
	{
		moveA(arr);
		return *this;
	}
	TArray &operator=(TArray &&arr)
	{
		moveA(arr);
		return *this;
	}
	void moveA(TArray &arr)
	{
		data = arr.data;
		arr.data = nullptr;
		size = arr.size;
		arr.size = 0;
		capacity = arr.capacity;
		arr.capacity = 0;
		lock = arr.lock;
		arr.lock = 0;
		magic = arr.magic;
		arr.magic = 0;
		flags = arr.flags;
		arr.flags = 0;
	}
	TT &operator()(int32_t index, Context *ctx)
	{
		if (index < 0 || uint64_t(index) >= size)
			ctx->throw_error_ex("array index out of range, %d of %llu", index,
					    (unsigned long long)size);
		return ((TT *)data)[index];
	}
	const TT &operator()(int32_t index, Context *ctx) const
	{
		if (index < 0 || uint64_t(index) >= size)
			ctx->throw_error_ex("array index out of range, %d of %llu", index,
					    (unsigned long long)size);
		return ((const TT *)data)[index];
	}
};

inline void array_lock(Context &, Array &arr, LineInfo *) { ++arr.lock; }
inline void array_unlock(Context &, Array &arr, LineInfo *)
{
	if (arr.lock) --arr.lock;
}

inline int builtin_array_size(const Array &arr) { return int(arr.size); }

inline void builtin_array_reserve(Array &arr, int newSize, int stride, Context *ctx, LineInfoArg *)
{
	if (newSize <= 0 || uint64_t(newSize) <= arr.capacity) return;
	char *nd = ctx->allocate(uint32_t(uint64_t(newSize) * uint32_t(stride)));
	if (arr.data) {
		memcpy(nd, arr.data, size_t(arr.size) * size_t(stride));
		ctx->free(arr.data, uint32_t(arr.capacity * uint32_t(stride)));
	}
	arr.data = nd;
	arr.capacity = uint64_t(newSize);
}

inline void builtin_array_resize(Array &arr, int newSize, int stride, Context *ctx, LineInfoArg *at)
{
	if (newSize < 0) newSize = 0;
	if (uint64_t(newSize) > arr.capacity)
		builtin_array_reserve(arr, newSize, stride, ctx, at);
	if (uint64_t(newSize) > arr.size && arr.data)
		memset(arr.data + arr.size * uint32_t(stride), 0,
		       size_t(uint64_t(newSize) - arr.size) * size_t(stride));
	arr.size = uint64_t(newSize);
}

template <typename TT>
struct das_auto_cast {
	template <typename QQ>
	static TT cast(QQ &&e)
	{
		return (TT)e;
	}
};

template <typename TT>
struct das_auto_cast_ref {
	template <typename QQ>
	static TT &cast(QQ &e)
	{
		return e;
	}
	template <typename QQ>
	static const TT &cast(const QQ &e)
	{
		return (const TT &)e;
	}
};

template <typename TT>
struct das_auto_cast_move {
	template <typename QQ>
	static TT cast(QQ &&e)
	{
		TT res = e;
		memset((void *)&e, 0, sizeof(QQ));
		return res;
	}
};

template <typename TT>
struct das_auto_cast_move<TT &> {
	template <typename QQ>
	static TT &cast(QQ &e)
	{
		return e;
	}
};

template <typename LT, typename RT>
struct das_ordered2 {
	LT left;
	RT right;
	das_ordered2(LT l, RT r) : left(static_cast<LT &&>(l)), right(static_cast<RT &&>(r)) {}
};
template <typename LT, typename RT>
das_ordered2(LT, RT) -> das_ordered2<LT, RT>;

template <typename TT, uint64_t mnh>
inline TT &das_global(Context *c)
{
	return *(TT *)(c->globals + c->globalOffsetByMangledName(mnh));
}

template <typename TT>
inline TT *das_ref(Context *, const TT &r)
{
	return &const_cast<TT &>(r);
}

template <typename TT>
inline TT &das_deref(Context *ctx, const TT *ptr, const char *file = "", int line = 0)
{
	if (!ptr) ctx->throw_error_ex("dereferencing null pointer at %s:%d", file, line);
	return *const_cast<TT *>(ptr);
}

template <typename TT>
inline void das_zero(TT &a)
{
	memset(&a, 0, sizeof(TT));
}

template <typename TT, typename QQ>
inline void das_move(TT &a, const QQ &b)
{
	if ((void *)&a != (void *)&b) {
		memcpy(&a, &b, sizeof(TT));
		memset((void *)&b, 0, sizeof(TT));
	}
}

template <typename TT, typename QQ>
inline void das_copy(TT &a, const QQ b)
{
	memcpy(&a, &b, sizeof(TT));
}

template <typename TT, typename QQ>
inline bool das_nequ_val(TT a, QQ b)
{
	return a != b;
}

inline void das_memcpy(void *left, const void *right, int size) { memcpy(left, right, size_t(size)); }

template <typename TT>
struct das_arg {
	static TT &pass(TT &&a) { return *((TT *)&a); }
	static TT &pass(TT &a) { return a; }
	static const TT &pass(const TT &&a) { return *((const TT *)&a); }
	static const TT &pass(const TT &a) { return a; }
};

template <typename TT>
struct das_cast {
	template <typename QQ>
	static TT &cast(const QQ &expr)
	{
		return reinterpret_cast<TT &>(const_cast<QQ &>(expr));
	}
};
template <typename TT>
struct das_cast<const TT> : das_cast<TT> {};

template <typename TT>
struct das_reinterpret {
	template <typename QQ>
	static TT &pass(QQ &&a)
	{
		return *((TT *)&a);
	}
	template <typename QQ>
	static TT &pass(QQ &a)
	{
		return *((TT *)&a);
	}
	template <typename QQ>
	static const TT &pass(const QQ &a)
	{
		return *((const TT *)&a);
	}
};
template <typename TT>
struct das_reinterpret<const TT> : das_reinterpret<TT> {};

template <typename TT>
struct das_iterator;

template <>
struct das_iterator<range> {
	explicit das_iterator(const range &r) : that(r) {}
	bool first(Context *, int32_t &i)
	{
		i = that.from;
		return i < that.to;
	}
	bool next(Context *, int32_t &i)
	{
		i++;
		return i != that.to;
	}
	void close(Context *, int32_t &) {}
	range that;
};

template <typename TT>
struct das_iterator<TArray<TT> const> {
	explicit das_iterator(const TArray<TT> &r)
	{
		that = &r;
		array_end = (const TT *)(r.data + r.size * sizeof(TT));
	}
	template <typename QQ>
	bool first(Context *ctx, const QQ *&i)
	{
		context = ctx;
		array_lock(*ctx, *(Array *)that, nullptr);
		i = (const QQ *)that->data;
		return i != (const QQ *)array_end;
	}
	template <typename QQ>
	bool next(Context *, const QQ *&i)
	{
		i++;
		return i != (const QQ *)array_end;
	}
	template <typename QQ>
	void close(Context *ctx, const QQ *&i)
	{
		context = nullptr;
		array_unlock(*ctx, *(Array *)that, nullptr);
		i = nullptr;
	}
	const Array *that = nullptr;
	const TT *array_end = nullptr;
	Context *context = nullptr;
};

template <typename TT, typename AT, bool moveIt = false>
struct das_ascend {
	static TT *make(Context *ctx, TypeInfo *typeInfo, const AT &init)
	{
		auto size = sizeof(AT) + (typeInfo ? 16 : 0);
		char *ptr = ctx->allocate(uint32_t(size));
		if (typeInfo) {
			*((TypeInfo **)ptr) = typeInfo;
			ptr += 16;
		}
		memcpy(ptr, &init, sizeof(AT));
		if (moveIt) memset((char *)&init, 0, sizeof(AT));
		return (TT *)ptr;
	}
};

template <typename AT>
struct das_ascend<Lambda, AT, false> {
	static Lambda make(Context *ctx, TypeInfo *typeInfo, const AT &init)
	{
		auto size = sizeof(AT) + (typeInfo ? 16 : 0);
		char *ptr = ctx->allocate(uint32_t(size));
		if (typeInfo) {
			*((TypeInfo **)ptr) = typeInfo;
			ptr += 16;
		}
		memcpy(ptr, &init, sizeof(AT));
		return Lambda(ptr);
	}
};

template <typename TT>
struct das_delete_lambda_struct;

template <typename TT>
struct das_delete_lambda_struct<TT *> {
	static void clear(Context *ctx, TT *ptr)
	{
		ctx->free(((char *)ptr) - 16, uint32_t(sizeof(TT) + 16));
	}
};

template <typename T>
struct SimPolicy {
	static T Div(T a, T b, Context &, LineInfo *) { return a / b; }
	static T Mod(T a, T b, Context &, LineInfo *) { return a % b; }
};

inline uint32_t uint32_clz(uint32_t x) { return x ? uint32_t(__builtin_clz(x)) : 32u; }

template <typename ResType, int methodOffset>
struct das_invoke_method {
	template <typename FirstArgType, typename... ArgType>
	static ResType invoke(Context *ctx, LineInfo *at, const FirstArgType &blk, ArgType... arg)
	{
		SimFunction *simFunc = ((Func *)((char *)&blk + methodOffset))->PTR;
		if (!simFunc) ctx->throw_error_at(at, "invoke null function");
		using fnPtrType = ResType (*)(Context *, const FirstArgType &, ArgType...);
		return ((fnPtrType)simFunc->aotFunction)(ctx, blk, static_cast<ArgType &&>(arg)...);
	}
	template <typename FirstArgType>
	static ResType invoke(Context *ctx, LineInfo *at, const FirstArgType &blk)
	{
		SimFunction *simFunc = ((Func *)((char *)&blk + methodOffset))->PTR;
		if (!simFunc) ctx->throw_error_at(at, "invoke null function");
		using fnPtrType = ResType (*)(Context *, const FirstArgType &);
		return ((fnPtrType)simFunc->aotFunction)(ctx, blk);
	}
	template <typename FirstArgType, typename... ArgType>
	static ResType invoke_cmres(Context *ctx, LineInfo *at, const FirstArgType &blk, ArgType... arg)
	{
		return invoke<FirstArgType, ArgType...>(ctx, at, blk, static_cast<ArgType &&>(arg)...);
	}
	template <typename FirstArgType>
	static ResType invoke_cmres(Context *ctx, LineInfo *at, const FirstArgType &blk)
	{
		return invoke<FirstArgType>(ctx, at, blk);
	}
};

template <int methodOffset>
struct das_invoke_method<void, methodOffset> {
	template <typename FirstArgType, typename... ArgType>
	static void invoke(Context *ctx, LineInfo *at, const FirstArgType &blk, ArgType... arg)
	{
		SimFunction *simFunc = ((Func *)((char *)&blk + methodOffset))->PTR;
		if (!simFunc) ctx->throw_error_at(at, "invoke null function");
		using fnPtrType = void (*)(Context *, const FirstArgType &, ArgType...);
		((fnPtrType)simFunc->aotFunction)(ctx, blk, static_cast<ArgType &&>(arg)...);
	}
	template <typename FirstArgType>
	static void invoke(Context *ctx, LineInfo *at, const FirstArgType &blk)
	{
		SimFunction *simFunc = ((Func *)((char *)&blk + methodOffset))->PTR;
		if (!simFunc) ctx->throw_error_at(at, "invoke null function");
		using fnPtrType = void (*)(Context *, const FirstArgType &);
		((fnPtrType)simFunc->aotFunction)(ctx, blk);
	}
};

template <typename ResType>
struct das_invoke_lambda {
	template <typename... ArgType>
	static ResType invoke(Context *ctx, LineInfo *at, const Lambda &blk, ArgType... arg)
	{
		SimFunction **fnpp = (SimFunction **)blk.capture;
		if (!fnpp) ctx->throw_error_at(at, "invoke null lambda");
		SimFunction *simFunc = *fnpp;
		if (!simFunc) ctx->throw_error_at(at, "invoke null function");
		using fnPtrType = ResType (*)(Context *, void *, ArgType...);
		return ((fnPtrType)simFunc->aotFunction)(ctx, blk.capture, static_cast<ArgType &&>(arg)...);
	}
};

struct SimNode_AotInteropBase : SimNode_CallBase {
	SimNode_AotInteropBase() : SimNode_CallBase(LineInfo(), "") {}
	vec4f *argumentValues = nullptr;
};

template <int argumentCount>
struct SimNode_AotInterop : SimNode_AotInteropBase {
	template <typename... VI>
	SimNode_AotInterop(TypeInfo **tinfo, VI... args)
	{
		nArguments = argumentCount;
		types = tinfo;
		argumentValues = argValues;
		vec4f argsE[argumentCount] = { args... };
		for (int i = 0; i != argumentCount; ++i) argumentValues[i] = argsE[i];
	}
	vec4f argValues[argumentCount];
};

inline char *das_string_builder_temp(Context *, const SimNode_AotInteropBase &node)
{
	static char buf[32];
	if (node.nArguments >= 1 && node.types && node.types[0] && node.types[0]->type == Type::tInt)
		snprintk(buf, sizeof(buf), "%d", node.argumentValues[0].i[0]);
	else
		buf[0] = 0;
	return buf;
}

template <typename TT>
struct cast {
	static vec4f from(const TT &v)
	{
		vec4f r;
		memset(&r, 0, sizeof(r));
		memcpy(&r, &v, sizeof(TT) < sizeof(r) ? sizeof(TT) : sizeof(r));
		return r;
	}
	static TT to(vec4f x) { return *reinterpret_cast<TT *>(&x); }
};

template <typename TT>
struct cast_aot_arg {
	static TT to(Context &, vec4f x) { return cast<TT>::to(x); }
};
template <typename TT>
struct cast_aot_arg<TT &> {
	static TT &to(Context &, vec4f)
	{
		static TT dummy{};
		return dummy;
	}
};
template <typename TT>
struct cast_aot_arg<const TT &> {
	static const TT &to(Context &, vec4f)
	{
		static TT dummy{};
		return dummy;
	}
};

struct SimNode_Aot : SimNode_CallBase {
	vec4f (*wrap)(Context *);
	SimNode_Aot(void *fn, vec4f (*w)(Context *)) : SimNode_CallBase(LineInfo(), ""), wrap(w)
	{
		aotFunction = fn;
	}
	vec4f eval(Context &c) override { return wrap(&c); }
};
struct SimNode_AotCMRES : SimNode_Aot {
	using SimNode_Aot::SimNode_Aot;
};

struct AotFactory {
	bool is_cmres = false;
	void *fn = nullptr;
	vec4f (*wrappedFn)(Context *) = nullptr;
	AotFactory() = default;
	AotFactory(bool c, void *f, vec4f (*w)(Context *)) : is_cmres(c), fn(f), wrappedFn(w) {}
	SimNode *operator()(Context &ctx) const
	{
		return is_cmres ? ctx.code->makeNode<SimNode_AotCMRES>(fn, wrappedFn)
				: ctx.code->makeNode<SimNode_Aot>(fn, wrappedFn);
	}
};

using AotLibrary = unordered_map<uint64_t, AotFactory>;

struct AotListBase {
	using Fn = void (*)(AotLibrary &);
	AotListBase(Fn f) : regFn(f)
	{
		tail = head;
		head = this;
	}
	static void registerAot(AotLibrary &lib)
	{
		for (auto *it = head; it; it = it->tail)
			it->regFn(lib);
	}
	inline static AotListBase *head = nullptr;
	AotListBase *tail = nullptr;
	Fn regFn = nullptr;
};

inline AotLibrary &getGlobalAotLibrary()
{
	static AotLibrary lib;
	static bool once = false;
	if (!once) {
		once = true;
		AotListBase::registerAot(lib);
	}
	return lib;
}
inline void clearGlobalAotLibrary() {}

inline uint64_t InitAotFunction(const Context &ctx, SimFunction *fn, const FunctionInfo &info)
{
	fn->name = ctx.code->allocateName(info.name);
	fn->mangledName = ctx.code->allocateName(info.mangledName);
	fn->stackSize = info.stackSize;
	fn->mangledNameHash = info.mnh;
	fn->fastcall = info.fastCall;
	fn->unsafe = info.unsafeOperation;
	fn->cmres = info.res_ref;
	fn->builtin = info.builtin && !info.promoted;
	fn->pinvoke = info.pinvoke;
	return info.mnh;
}

inline SizeDiff InitGlobalVariable(const Context &ctx, GlobalVariable *g, const GlobalVarInfo &info)
{
	g->name = ctx.code->allocateName(info.name);
	g->size = info.typeSize;
	g->mangledNameHash = info.mangledNameHash;
	g->offset = (uint32_t)ctx.getGlobalSize();
	return {0, (uint64_t(g->size) + 0xfu) & ~0xfull};
}

inline void InitGlobalVar(Context &ctx, GlobalVariable *g, GlobalVarInfo info)
{
	auto d = InitGlobalVariable(ctx, g, info);
	ctx.updateSharedGlobalSize(d.sharedSizeDiff, d.globalsSizeDiff);
}

inline void FillFunction(Context &ctx, const AotLibrary &lib, vector<pair<uint64_t, SimFunction *>> &fns)
{
	for (int i = 0; i < fns.n; ++i) {
		auto it = lib.find(fns.d[i].first);
		if (it == lib.end()) continue;
		auto *fn = fns.d[i].second;
		fn->code = (it->second)(ctx);
		fn->aot = true;
		fn->aotFunction = ((SimNode_CallBase *)fn->code)->aotFunction;
		(*ctx.tabMnLookup)[fn->mangledNameHash] = fn;
	}
}

struct StandaloneContextNode {
	StandaloneContextNode(Context *(*)()) {}
};

} // namespace das
