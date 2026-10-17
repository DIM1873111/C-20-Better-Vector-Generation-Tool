#ifndef BETTER_VEC
#define BETTER_VEC

#include <vector>
#include <algorithm>
#include <ranges>
#include <functional>
#include <type_traits>
#include <tuple>
#include <memory>
#include <concepts>

#if defined(__has_include)
#   if __has_include("thread_tool.hpp")
#       ifndef THREADED_CONTAINER_OPS
#           define THREADED_CONTAINER_OPS
#           include "thread_tool.hpp"
#       endif
#   endif
#endif

namespace vecUtils{
    namespace FunWrap{

        template<class F>       
        auto repeatSecond(F&& f){
            return [f]<typename T>
            (std::vector<int>& labelResult,const std::vector<T>& vec){
                size_t size = vec.size();    //获取大小
                for(size_t i = 0; i < size; i++){
                    for(size_t j = size - 1; j > i; j--){
                        if(f(vec[i], vec[j])){
                            labelResult.push_back(i);
                            break;
                        }
                    }
                }
            };
        }

        template<class F>
        auto allDuplicates(F&& f){
            return [f]<typename T>
            (std::vector<int>& labelResult,const std::vector<T>& vec){
                size_t size = vec.size();
                for(size_t i = 0; i < size; i++){
                    for(size_t j = 0; j < size;j++){
                        if(!(j == i) && f(vec[i], vec[j])){
                            labelResult.push_back(i);
                            break;
                        }
                    }
                }
            };
        }
        template<typename ClassT, typename RteT, typename... Args>
        auto MemberFunc(ClassT* p, RteT(ClassT::*pmf)(Args...)){
            return [p, pmf](Args&&... args)->decltype(auto){
                return (p->*pmf)(std::forward<Args>(args)...);
            };
        }
        template<typename ClassT, typename RteT, typename... Args>
        auto MemberFunc(const ClassT* p, RteT(ClassT::*pmf)(Args...) const){
            return [p, pmf](Args&&... args){
                return (p->*pmf)(std::forward<Args>(args)...);
            };
        }

    };

    namespace detail{

        template<typename T>
        struct is_vector : std::false_type {};

        template<typename T, typename Alloc>
        struct is_vector<std::vector<T, Alloc>> : std::true_type {};

        template<typename T>
        struct is_vector<const T> : is_vector<T> {};

        template<typename T>
        struct is_vector<volatile T> : is_vector<T> {};

        template<typename T>
        inline constexpr bool is_vector_v = is_vector<T>::value;

        template<typename Container>
        using vector_element_t = std::conditional_t<
            std::is_const_v<Container>,
            const typename std::remove_cv_t<Container>::value_type,
            typename std::remove_cv_t<Container>::value_type
        >;

        template<typename T>
        struct Vec_Type{
            static_assert(is_vector_v<T>, "Non vector types cannot be operated on");
            static constexpr bool is_const = std::is_const_v<T>;
            using ElementT = vector_element_t<T>;
        };

        template<typename F, typename Param>
        inline constexpr bool is_ReferenceReturn = std::is_reference_v<std::invoke_result_t<F,Param>>;
        
        template<typename... Args>
        struct LastTypeHelper {
            using type = std::tuple_element_t<
                (sizeof...(Args) == 0 ? 0 : sizeof...(Args) - 1),
                std::tuple<Args..., void>
                >;
        };
        template<typename... Args>
        struct FirstTypeHelper{
            using type = std::tuple_element_t<0, std::tuple<Args..., void>>;
        };
        /*Function return|parameter type extraction specialization*/
        template<typename... Args>
        using ArgsLastType = typename LastTypeHelper<Args...>::type;

        template<typename... Args>
        using ArgsFirstType = typename FirstTypeHelper<Args...>::type;

        template<class F> struct function_traits;
        template<class R, class... A>
        struct function_traits<R(A...)> {
            using return_type = R;
            using args = std::tuple<A...>;
            static constexpr std::size_t arity = sizeof...(A);
        };
        template<class R, class... A> struct function_traits<R(*)(A...)> : function_traits<R(A...)> {};
        template<class R, class... A> struct function_traits<R(&)(A...)> : function_traits<R(A...)> {};
        template<class C, class R, class... A> struct function_traits<R(C::*)(A...)>       : function_traits<R(A...)> {};
        template<class C, class R, class... A> struct function_traits<R(C::*)(A...) const> : function_traits<R(A...)> {};
        template<class F> requires requires { &F::operator(); }
        struct function_traits<F> : function_traits<decltype(&F::operator())> {};
        template<class F> using ret_t  = typename function_traits<F>::return_type;
        template<class F> using args_t = typename function_traits<F>::args;
        template<class F> inline constexpr std::size_t arity_v = function_traits<F>::arity;
        template<class F, std::size_t I> using arg_t = std::tuple_element_t<I, args_t<F>>;
        template<class F, class E, bool HasArg = (function_traits<F>::arity >= 1)>
        struct first_arg_is : std::false_type {};
        template<class F, class E>
        struct first_arg_is<F, E, true> : std::is_same<std::remove_cvref_t<arg_t<F, 0>>, std::remove_cvref_t<E>>{};
        template<class F, class E>
        inline constexpr bool first_arg_is_v = first_arg_is<F, E>::value;


        template <class T, class = void>
        struct arg1;

        template <template <class...> class TT, class R>
        struct arg1<TT<R>> { using type = R; };

        template <class T>
        using arg1_t = typename arg1<T>::type;


        template<class P>
        inline constexpr bool is_modify_proc = std::is_void_v<ret_t<P>> || std::is_reference_v<ret_t<P>>;

        template<template<class...> class T, class... Args>
        std::true_type  is_spec_probe(T<Args...>*);

        template<template<class...> class T>
        std::false_type is_spec_probe(...);

        template<class X, template<class...> class T>
        inline constexpr bool is_spec_of_v =
            decltype(is_spec_probe<T>(static_cast<std::remove_cvref_t<X>*>(nullptr)))::value;


        template<class DataT, class Func>
        decltype(auto) funcFusion_help(DataT&& data, Func&& f){
            return std::forward<Func>(f)(std::forward<DataT>(data));
        }
        template<class DataT, class Func ,class... FuncParams>
        decltype(auto) funcFusion_help(DataT&& data, Func&& firstFunc ,FuncParams&&... param){
            static_assert(requires{std::forward<Func>(firstFunc)(std::forward<DataT>(data));}, "The function return value is incompatible with the next function.");
            return funcFusion_help(firstFunc(std::forward<DataT>(data)), std::forward<FuncParams>(param)...);
        }

        template<class... F_P>
        auto funcFusion(F_P&&... Funcparam){
            using FirstF = std::remove_cvref_t<vecUtils::detail::ArgsFirstType<F_P...>>;
            using ElemT  = std::remove_reference_t<vecUtils::detail::arg_t<FirstF, 0>>;
            return [...FuncList = std::forward<F_P>(Funcparam)](ElemT& data) -> decltype(auto) {
                return funcFusion_help(data, FuncList...);
            };
        }

    };

    namespace Controller{
            template<typename,bool> class OpAgent;  
            template<typename RangeF, typename... Params>
            class GenOpProxy{
                private:
                    template<typename,bool> friend class OpAgent;
                    template<typename, typename...> friend class GenOpProxy;

                    RangeF loopFunc;
                    std::tuple<Params...> flowList;

                    using funcType = detail::ArgsLastType<Params...>;
                    using funVarT = detail::ArgsFirstType<Params...>;

                    using funcReturnType = detail::ret_t<
                    std::conditional_t<std::is_same_v<funcType,void>,void(),funcType>>;
                    
                    template<std::size_t... Is>
                    auto make_next(auto&& processorFunc, std::index_sequence<Is...>){
                        using CleanFunc = std::decay_t<decltype(processorFunc)>;
                        return GenOpProxy<RangeF, Params..., CleanFunc>{
                            std::forward<RangeF>(loopFunc),
                            std::get<Is>(flowList)...,
                            std::forward<CleanFunc>(processorFunc)       
                        };
                    }
                    template<bool IsFirst>
                    auto createVecProcessor(auto&& processor){
                        return [this, f = std::move(processor)](auto&& vec)mutable -> decltype(auto){
                            using fRet = detail::ret_t<decltype(f)>;
                            constexpr bool is_modify = std::is_reference_v<fRet> || std::is_void_v<fRet>;

                            if constexpr(IsFirst){
                                if constexpr(is_modify){
                                    loopFunc(&vec)(f);
                                    return std::forward<decltype(vec)>(vec);
                                }else{
                                    std::vector<std::remove_reference_t<fRet>> tmpvec;
                                    tmpvec.reserve(vec.size());
                                    loopFunc(&vec)([f = std::move(f),&tmpvec](auto&& i){
                                        tmpvec.push_back(f(i));
                                    });
                                    return tmpvec;
                                }
                            }else{
                                if constexpr(is_modify){
                                    for(auto& i : vec) f(i);
                                    return std::forward<decltype(vec)>(vec);
                                }else{
                                    std::vector<std::remove_reference_t<fRet>> tmpvec;
                                    tmpvec.reserve(vec.size());
                                    for(auto& i : vec) tmpvec.push_back(f(i));
                                    return tmpvec;
                                }
                            }
                        };
                    };
                    template<std::size_t I, class A>
                    decltype(auto) rec(A&& acc){
                        if constexpr (std::is_lvalue_reference_v<A&&>){
                            return std::forward<A>(acc);
                        }else{
                            return std::remove_cvref_t<A>(std::forward<A>(acc));
                        }
                    }
                    template<std::size_t I, class dataT, class first, class... R>
                    decltype(auto) rec(dataT&& data, first&& var, R&&... processorList){
                        return rec<I + 1>  //SplicingProcessor...
                        (createVecProcessor<I == 0>
                        (std::forward<first>(var))
                        (std::forward<dataT>(data)),
                        std::forward<R>(processorList)...); 
                    }
                    template<typename VecT>
                    decltype(auto) send(VecT&& vecP){
                        return std::apply([this, &vecP](auto&&... processorList)->decltype(auto){
                            return rec<0>(std::forward<VecT>(vecP), std::forward<decltype(processorList)>(processorList)...);
                        },flowList);
                    }
#               ifdef THREADED_CONTAINER_OPS

                    template<bool IsFirst>
                    auto createThreadVecProcessor(auto&& processor){
                        return [this, f = std::move(processor)](auto&& vec)mutable -> decltype(auto){
                            using fRet = detail::ret_t<decltype(f)>;

                            constexpr bool is_modify = std::is_reference_v<fRet> || std::is_void_v<fRet>;
                            using FirstParam = std::remove_reference_t<detail::arg_t<decltype(f), 0>>;

                            if constexpr(IsFirst){
                                if constexpr(is_modify){
                                    std::vector<FirstParam*> targetPtr;
                                    targetPtr.reserve(vec.size());

                                    loopFunc(&vec)([&targetPtr](auto&& i){
                                        targetPtr.push_back(&i);
                                    });
                                    ThreadTool::oneTimeThread([f](FirstParam* i){ //void
                                        f(*i);
                                    },targetPtr);
                                    return std::forward<decltype(vec)>(vec);
                                }else{
                                    std::vector<FirstParam*> targetPtr;
                                    targetPtr.reserve(vec.size());

                                    loopFunc(&vec)([&targetPtr](auto&& i){
                                        targetPtr.push_back(&i);
                                    });
                                    return ThreadTool::oneTimeThread([f](FirstParam* i){
                                        return f(*i);
                                    },targetPtr);
                                }
                            }else{
                                if constexpr(is_modify){
                                    ThreadTool::oneTimeThread(f,vec);
                                    return std::forward<decltype(vec)>(vec);
                                }else{
                                    return ThreadTool::oneTimeThread(f,vec);
                                }
                            }
                        };
                    }

                    template<std::size_t I, class A>
                    decltype(auto) threadRec(A&& acc){
                        if constexpr (std::is_lvalue_reference_v<A&&>){
                            return std::forward<A>(acc);
                        }else{
                            return std::remove_cvref_t<A>(std::forward<A>(acc));
                        }
                    }
                    template<std::size_t I, class dataT, class first, class... R>
                    decltype(auto) threadRec(dataT&& data, first&& var, R&&... processorList){
                        return threadRec<I + 1>
                        (createThreadVecProcessor<I == 0>
                        (std::forward<first>(var))
                        (std::forward<dataT>(data)), 
                        std::forward<R>(processorList)...);
                    }
                    template<typename VecT>
                    decltype(auto) threadSend(VecT&& vecP){
                        return std::apply([this, &vecP](auto&&... processorList)->decltype(auto){
                            return threadRec<0>(std::forward<VecT>(vecP), std::forward<decltype(processorList)>(processorList)...);
                        },flowList);
                    }
#               endif
                    template<class F, typename... Args>
                        requires(!std::is_same_v<std::remove_cvref_t<F>, GenOpProxy>)
                    GenOpProxy(F&& f, Args&&... param):
                    loopFunc(std::forward<F>(f)),
                    flowList(std::forward<Args>(param)...){}
                public:

                    template<class F>
                        requires (!std::is_same_v<std::remove_cvref_t<F>, GenOpProxy>)
                    GenOpProxy(F&& f):loopFunc(std::forward<F>(f)){}

                    auto operator|(auto&& processorFunc)
                    requires(sizeof...(Params) == 0 || (!std::is_same_v<funcType, void> 
                        && detail::first_arg_is_v<std::remove_cvref_t<decltype(processorFunc)>, funcReturnType>)){
                        return make_next(std::forward<decltype(processorFunc)>(processorFunc),
                            std::index_sequence_for<Params...>{});
                    }
            };
            template<typename T, bool TAKEOVER = false>
            class OpAgent{
                private:
                    static constexpr bool is_const = detail::Vec_Type<T>::is_const;
                    using VecIt = std::conditional_t<is_const,typename T::const_iterator,typename T::iterator>; 
                    using ElementT = typename detail::Vec_Type<T>::ElementT;
            
                    std::unique_ptr<T> owned = nullptr;
                    T* vec;

                    template<class dataT>
                    decltype(auto) multiPipeHandler(dataT&& vec){
                        if constexpr(std::is_lvalue_reference_v<dataT&&>){
                            return std::forward<dataT>(vec);
                        }else{
                            return std::remove_cvref_t<dataT>(std::move(vec));
                        }
                    }
                    template<class dataT, class firstT, class... routeT>
                    decltype(auto) multiPipeHandler(dataT&& vecP, firstT&& first, routeT&&... route){
                        static_assert(
                            detail::is_spec_of_v<firstT, GenOpProxy> 
                            && (detail::is_spec_of_v<routeT, GenOpProxy> && ...), 
                            "Incorrect pipe type"
                        );
                        return multiPipeHandler(first.send(std::forward<dataT>(vecP)), std::forward<routeT>(route)...);
                    }

#               ifdef THREADED_CONTAINER_OPS

                    template<class dataT>
                    decltype(auto) threadMultiPipeHandler(dataT&& vec){
                        if constexpr(std::is_lvalue_reference_v<dataT&&>){
                            return std::forward<dataT>(vec);
                        }else{
                            return std::remove_cvref_t<dataT>(std::move(vec));
                        }
                    }
                    template<class dataT, class firstT, class... routeT>
                    decltype(auto) threadMultiPipeHandler(dataT&& vecP, firstT&& first, routeT&&... route){
                        static_assert(
                            detail::is_spec_of_v<firstT, GenOpProxy> 
                            && (detail::is_spec_of_v<routeT, GenOpProxy> && ...), 
                            "Incorrect pipe type"
                        );
                        return threadMultiPipeHandler(first.threadSend(std::forward<dataT>(vecP)), std::forward<routeT>(route)...);
                    }

#               endif



                    decltype(auto) getOp(){
                        if constexpr(TAKEOVER){
                            return std::move(*vec);
                        }else{
                            return *vec;
                        }
                    }


                public:
                    OpAgent(T* v):vec(v){}
                    OpAgent(T&& v):owned(std::make_unique<T>(std::move(v))),vec(owned.get()){}
                    OpAgent(OpAgent&&)            = delete; // 禁用移动构造
                    OpAgent& operator=(OpAgent&&) = delete; // 禁用移动赋值

                    /**
                     * @brief 返回移动元素
                     */
                    auto Move()
                    requires(!is_const){
                        return std::move(*vec);
                    }
                    /**
                     * @brief 返回复制元素
                     */
                    auto Copy(){
                        return *vec;
                    }
                    /**
                     * @brief 元素搜索(指针)
                     * @param f(T&) 搜索函数
                     */
                    template<class F>    
                    auto ptrSearch(F&& f){
                        auto it = std::find_if((*vec).begin(), (*vec).end(), std::forward<F>(f));
                        return (it == (*vec).end()) ? nullptr : std::addressof(*it);
                    }
                    /**
                     * @brief 元素搜索(iterator)
                     * @param f(T&) 搜索函数
                     */
                    template<class F>    
                    auto ItSearch(F&& f){
                        return std::find_if((*vec).begin(), (*vec).end(), std::forward<F>(f));
                    }
                    /**
                     * Generation
                     * 
                     */
                    template<class... Proxies>
                    decltype(auto) iterator(Proxies... params){
                        return multiPipeHandler(getOp(), std::move(params)...);
                    }
#               ifdef THREADED_CONTAINER_OPS
                    template<class... Proxies>
                    decltype(auto) threadIterator(Proxies... params){
                        return threadMultiPipeHandler(getOp(), std::move(params)...);
                    }
#               endif

                    /**
                     * @brief 返回迭代器的末尾位置
                     * @param position 默认(vec.end()+0)
                     */
                    auto ItEnd(size_t position = 0){
                        return (*vec).end() - position;
                    }
                    /**
                     * @brief 返回迭代器的开始位置
                     * @param position 默认(vec.begin()+0)
                     */
                    auto ItBegin(size_t position = 0){
                        return (*vec).begin() + position;
                    }
                    /**
                     * @brief 移动合并
                     * @param moveVec 需要合并的对象
                     * 
                     * @note 会对目标对象移动
                     */
                    auto MoveMerge(T& moveVec)
                    requires(!is_const){
                        return (*vec).insert((*vec).end(),
                        std::make_move_iterator(moveVec.begin()),
                        std::make_move_iterator(moveVec.end()));   
                    }
                    /**
                     * @brief 移动合并
                     * @param moveVec 需要合并的对象
                     * @param range 插入位置
                     * 
                     * @note 会对目标对象移动
                     */
                    auto MoveMerge(T& moveVec ,VecIt range)
                    requires(!is_const){
                        return (*vec).insert(range,
                        std::make_move_iterator(moveVec.begin()),
                        std::make_move_iterator(moveVec.end()));   
                    }

                    /**
                     * @brief 复制合并
                     * @param moveVec 需要合并的对象
                     * 
                     */
                    auto CopyMerge(const T& copyVec)
                    requires(!is_const){
                        return (*vec).insert((*vec).end(),
                            copyVec.begin(),
                            copyVec.end());   
                    }
                    /**
                     * @brief 复制合并
                     * @param moveVec 需要合并的对象
                     * @param range 插入位置
                     * 
                     */
                    auto CopyMerge(const T& copyVec ,VecIt range)
                    requires(!is_const){
                        return (*vec).insert(range,
                            copyVec.begin(),
                            copyVec.end());   
                    }
                
                    /**
                     * @brief 通过函数查找符合要求的对象
                     * @param f 查找对象函数
                     * 
                     * @note 返回std::vector<int>(标记的对象下标)
                     * 
                     */
                    template<class F>
                    auto findIdx(F&& f){
                        std::vector<int> elementMark;
                        f(elementMark, *vec);
                        return elementMark;
                    }

            };  
            //模板
            template<typename T>
            static OpAgent<std::vector<T>> control(std::vector<T>& vec){
                return OpAgent<std::vector<T>>(&vec);
            }
            template<typename T>
            static OpAgent<const std::vector<T>> control(const std::vector<T>& vec){
                return OpAgent<const std::vector<T>>(&vec);
            }
            template<typename T>
            static OpAgent<std::vector<T>, true> control(std::vector<T>&& vec){
                return OpAgent<std::vector<T>, true>(std::move(vec));
            }

            static auto range(auto&& range){
                return GenOpProxy<std::remove_cvref_t<decltype(range)>>(
                    std::forward<decltype(range)>(range));
            }
            static auto sendAll(){
                return [](auto* vec){
                    return [vec](auto&& f){
                        for(auto& i : *vec) f(i);
                    };
                };
            }
            //template<typename...> 
            //template< <typename...>typename containerT>

            template<std::convertible_to<size_t>... Rest>
            static auto sendIndex(Rest... index){
                return [...index = static_cast<size_t>(index)](auto* vec){
                    return [vec, index...](auto&& f){
                        (f((*vec)[index]),...);
                    };
                };
            }
            template<std::ranges::input_range R>
            static auto sendIndex(R index)
            requires(std::convertible_to<std::ranges::range_value_t<R>, std::size_t>){
                return [index = std::move(index)](auto* vec){
                    return [vec, index](auto&& f){
                        for(const auto& i : index) f((*vec)[i]);
                    };
                };
            }
            static auto sendRange(auto begin, auto end){
                return [begin, end](auto* vec){
                    return [begin, end](auto&& f){
                        for(auto it = begin; it != end; ++it) f(*it);
                    };
                }; 
            }

            static auto sendIf(auto&& conditionFunc){
                return [conf = std::move(conditionFunc)](auto* vec){
                    return [conf,vec](auto&& f){
                        for(auto& i : *vec) if(conf(i)) f(i);
                    };
                };
            }
            // static auto 

    };
};

namespace vecExt{
    using vecUtils::Controller::control;
    using vecUtils::Controller::range;
    using vecUtils::Controller::sendAll;
    using vecUtils::Controller::sendIndex;
    using vecUtils::Controller::sendIf;
    using vecUtils::Controller::OpAgent;
    using vecUtils::detail::funcFusion;
    namespace FunWrap = vecUtils::FunWrap;
};


#endif //BETTER_CONTAINER