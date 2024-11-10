#include <mutex>
#include <set>
#include <atomic>
#include <optional>
#include <type_traits>
#include <boost/intrusive_ptr.hpp>


class NullMutex
{	public:
	void lock(void)
	{
	}
	void unlock(void)
	{
	}
	bool try_lock(void)
	{	return true;
	}
};

template<typename T, bool BTHREADED = true>
class unique:public T
{	typedef typename std::conditional<
		BTHREADED,
		std::atomic<std::size_t>,
		std::size_t
	>::type REFCOUNT;
	typedef typename std::conditional<
		BTHREADED,
		std::recursive_mutex,
		NullMutex
	>::type MUTEX;
	mutable REFCOUNT m_sRefCount;
	public:
	template<typename ...ARGS>
	unique(ARGS&&..._r)
		:T(std::forward<ARGS>(_r)...),
		m_sRefCount(std::size_t())
	{
	}
	struct compare
	{	bool operator()(const unique<T, BTHREADED>*const _p0, const unique<T, BTHREADED>*const _p1) const
		{	return *_p0 < *_p1;
		}
	};
	private:
	typedef std::set<const unique<T, BTHREADED>*, compare> SET;
	typedef std::pair<SET, MUTEX> setAndMutex;
	static setAndMutex&getSet(void)
	{	static setAndMutex s;
		return s;
	}
	public:
	template<typename ...ARGS>
	static boost::intrusive_ptr<const unique<T, BTHREADED> > create(ARGS&&..._r)
	{	const auto s = boost::intrusive_ptr<const unique<T, BTHREADED> >(new unique<T, BTHREADED>(std::forward<ARGS>(_r)...));
		std::lock_guard<MUTEX> sLock(getSet().second);
		return *getSet().first.insert(s.get()).first;
	}
	private:
	friend void intrusive_ptr_add_ref(const unique<T, BTHREADED>* const _p) noexcept
	{	std::lock_guard<MUTEX> sLock(getSet().second);
		++_p->m_sRefCount;
	}
	friend void intrusive_ptr_release(const unique<T, BTHREADED>* const _p) noexcept
	{	std::optional<std::lock_guard<MUTEX> > sLock(getSet().second);
		if (!--_p->m_sRefCount)
		{	getSet().first.erase(_p);
			sLock.reset();
			delete _p;
		}
	}
};

struct integerConstant
{	const int m_i;
	integerConstant(const int _i)
		:m_i(_i)
	{
	}
	bool operator<(const integerConstant&_r) const
	{	return m_i < _r.m_i;
	}
};

#include <vector>
#include <thread>


int main(int argc, char**argv)
{	const auto sCreate = [&](void)
	{	std::vector<boost::intrusive_ptr<const unique<integerConstant> > > sMap;
		sMap.reserve(std::atoi(argv[1]));
		for (int i = 0; i < sMap.capacity(); ++i)
			sMap.emplace_back(unique<integerConstant>::create(i));
	};
	std::vector<std::thread> sThreads;
	sThreads.reserve(std::size_t(std::atoi(argv[2])));
	for (std::size_t i = 0; i < sThreads.capacity(); ++i)
		sThreads.emplace_back(sCreate);
	while (!sThreads.empty())
	{	sThreads.back().join();
		sThreads.pop_back();
	}
}
