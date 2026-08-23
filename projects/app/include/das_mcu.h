#pragma once

#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <new>

inline void *operator new(size_t, void *p) noexcept { return p; }
inline void operator delete(void *, void *) noexcept {}

#define DAS_API
#define __forceinline inline
#define DAS_ASSERT(x) ((void)0)
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
	mutable Node nodes[32]{};
	struct iterator {
		Node *p = nullptr;
		Node *operator->() const { return p; }
		bool operator!=(iterator o) const { return p != o.p; }
		bool operator==(iterator o) const { return p == o.p; }
	};
	iterator end() const { return {const_cast<Node *>(nodes + 32)}; }
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

enum Type : int32_t { none, tInt = 17, tVoid = 9, tString = 33, tHandle = 51, tPointer = 65 };

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

struct LineInfo {};
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
};

struct NodeAllocator {
	char buf[2048]{};
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
	vec4f *abiArguments() { return abiArg; }

	vec4f abiArgBuf[8]{};
	vec4f *abiArg = abiArgBuf;
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

template <typename TT>
struct das_auto_cast {
	template <typename QQ>
	static TT cast(QQ &&e) { return (TT)e; }
};

template <typename TT, uint64_t mnh>
inline TT &das_global(Context *c)
{
	return *(TT *)(c->globals + c->globalOffsetByMangledName(mnh));
}

template <typename TT>
inline TT *das_ref(Context *, const TT &r) { return &const_cast<TT &>(r); }

template <typename TT>
inline void das_zero(TT &a) { memset(&a, 0, sizeof(TT)); }

template <typename TT, typename QQ>
inline void das_copy(TT &a, const QQ b) { memcpy(&a, &b, sizeof(TT)); }

template <typename TT, typename QQ>
inline bool das_nequ_val(TT a, QQ b) { return a != b; }

template <typename TT>
struct das_reinterpret {
	template <typename QQ>
	static TT &pass(QQ &&a) { return *((TT *)&a); }
	template <typename QQ>
	static TT &pass(QQ &a) { return *((TT *)&a); }
	template <typename QQ>
	static const TT &pass(const QQ &a) { return *((const TT *)&a); }
};
template <typename TT>
struct das_reinterpret<const TT> : das_reinterpret<TT> {};

template <typename TT>
struct cast {
	static vec4f from(TT v)
	{
		vec4f r;
		memset(&r, 0, sizeof(r));
		r.i[0] = (int32_t)v;
		return r;
	}
	static TT to(vec4f x) { return *(TT *)&x; }
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
