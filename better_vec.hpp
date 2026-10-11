#ifndef BETTER_VEC
#define BETTER_VEC

#include <vector>
#include <algorithm>
#include <ranges>
//导入函数容器
#include <functional>
#include <type_traits>

#include <tuple>
#include <memory>

//命名空间
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

    };//CleanFunc 
//using ArgsLastType = std::tuple_element_t<sizeof...(Args)-1, std::tuple<Args...>>;



    namespace Controller{
            template<typename T> class OpAgent;  
            template<typename RangeF, typename... Params>
            class GenOpProxy{
                private:
                    template<typename> friend class OpAgent;
                    template<typename, typename...> friend class GenOpProxy;

                    RangeF loopFunc;
                    std::tuple<Params...> flowList;

                    using funcType = detail::ArgsLastType<Params...>;
                    using funVarT = detail::ArgsFirstType<Params...>;

                    using funcReturnType = detail::ret_t<
                    std::conditional_t<std::is_same_v<funcType,void>,void(),funcType>>;

                    template<std::size_t... Is>
                    auto make_next(auto&& processorFunc, std::index_sequence<Is...>) {
                        using CleanFunc = std::decay_t<decltype(processorFunc)>;
                        return GenOpProxy<RangeF, Params..., CleanFunc>{
                            std::forward<RangeF>(loopFunc),
                            std::get<Is>(flowList)...,
                            std::forward<CleanFunc>(processorFunc)       
                        };
                    }
                    /**
                     * !! Control range → first function’s range >
                     * !! The nth function uses the vector container returned by the first function
                     * :: Because the user-provided loop encompasses the original container’s range
                     * :: and it is incompatible with the containers returned by subsequent functions, those later functions must rely on the container produced by the preceding one
                     * 
                     * * Check whether the index belongs to the first function
                     * * If so, process it using the user-supplied loopFunc; otherwise, use the default
                     */
                    template<bool IsFirst>
                    auto createVecProcessor(auto&& processor){
                        return [this, f = std::move(processor)](auto&& vec)mutable -> decltype(auto){
                            using fRet = detail::ret_t<decltype(f)>;

                            constexpr bool is_reference = std::is_reference_v<fRet>;

                            if constexpr(IsFirst){
                                if constexpr(is_reference){
                                    loopFunc(&vec)(f);
                                    return std::forward<decltype(vec)>(vec);
                                }else{
                                    std::vector<std::remove_reference_t<fRet>> tmpvec;
                                    loopFunc(&vec)([f = std::move(f),&tmpvec](auto&& i){
                                        tmpvec.push_back(f(i));
                                    });
                                    return tmpvec;
                                }
                            }else{
                                if constexpr(is_reference){
                                    for(auto& i : vec) f(i);
                                    return std::forward<decltype(vec)>(vec);
                                }else{
                                    std::vector<std::remove_reference_t<fRet>> tmpvec;
                                    for(auto& i : vec) tmpvec.push_back(f(i));
                                    return tmpvec;
                                }
                            }
                        };
                    };

                    template<std::size_t I, class A>
                    auto rec(A acc){return acc;}
                    template<std::size_t I, class dataT, class first, class... R>
                    decltype(auto) rec(dataT&& data, first&& var, R&&... processorList){
                        return rec<I + 1>  //SplicingProcessor...
                        (createVecProcessor<I == 0>
                        (std::forward<first>(var))
                        (std::forward<dataT>(data)),
                        std::forward<R>(processorList)...); 
                    }

                    /**
                     *  Concatenation loop function
                     * ((DIM_) | UnlimitedProgress) ↵
                     */
                    template<typename VecT>
                    decltype(auto) send(VecT* vecP){
                        return std::apply([this, vecP](auto&&... processorList)->decltype(auto){
                            return rec<0>(*vecP, std::forward<decltype(processorList)>(processorList)...);
                        },flowList);
                    }

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
            template<typename T>
            class OpAgent{
                private:
                    static constexpr bool is_const = detail::Vec_Type<T>::is_const;
                    using VecIt = std::conditional_t<is_const,typename T::const_iterator,typename T::iterator>; 
                    using ElementT = typename detail::Vec_Type<T>::ElementT;
            
                    std::unique_ptr<T> owned = nullptr;
                    T* vec;
                public:
                    OpAgent(T* v):vec(v){}
                    OpAgent(T&& v):owned(std::make_unique<T>(std::move(v))),vec(owned.get()){}
                    OpAgent(OpAgent&&)            = delete; // 禁用移动构造
                    OpAgent& operator=(OpAgent&&) = delete; // 禁用移动赋值

                    /**
                     * @brief Return the moved element
                     */
                    auto Move()
                    requires(!is_const){
                        return std::move(*vec);
                    }
                    /**
                     * @brief Return copied element
                     */
                    auto Copy(){
                        return *vec;
                    }
                    /**
                     * @brief Element Search(Pointer)
                     * @param f(T&) Search function
                     */
                    template<class F>    
                    auto ptrSearch(F&& f){
                        auto it = std::find_if((*vec).begin(), (*vec).end(), std::forward<F>(f));
                        return (it == (*vec).end()) ? nullptr : std::addressof(*it);
                    }
                    /**
                     * @brief Element Search(iterator)
                     * @param f(T&) Search function
                     */
                    template<class F>    
                    auto ItSearch(F&& f){
                        return std::find_if((*vec).begin(), (*vec).end(), std::forward<F>(f));
                    }
                    /**
                     * Generation
                     * 
                     */
                    template<class RangeF, class... Params>
                    decltype(auto) iterator(GenOpProxy<RangeF, Params...>&& opProxy){
                        return opProxy.send(vec);
                    }
                    template<class RangeF, class... Params>
                    decltype(auto) iterator(GenOpProxy<RangeF, Params...>& opProxy){
                        return opProxy.send(vec);
                    }
                    /**
                     * @brief Returns the end position of the iterator.
                     * @param position Default(vec.end()+0)
                     */
                    auto ItEnd(size_t position = 0){
                        return (*vec).end() - position;
                    }
                    /**
                     * @brief Return the beginning of the iterator.
                     * @param position Default(vec.begin()+0)
                     */
                    auto ItBegin(size_t position = 0){
                        return (*vec).begin() + position;
                    }
                    /**
                     * @brief Mobile Merge
                     * @param moveVec Objects to be merged
                     * 
                     * @note Will move toward the target object.
                     */
                    auto MoveMerge(T& moveVec)
                    requires(!is_const){
                        return (*vec).insert((*vec).end(),
                        std::make_move_iterator(moveVec.begin()),
                        std::make_move_iterator(moveVec.end()));   
                    }
                    /**
                     * @brief Mobile Merge
                     * @param moveVec Objects to be merged
                     * @param range Insertion Position
                     * 
                     * @note Will move toward the target object.
                     */
                    auto MoveMerge(T& moveVec ,VecIt range)
                    requires(!is_const){
                        return (*vec).insert(range,
                        std::make_move_iterator(moveVec.begin()),
                        std::make_move_iterator(moveVec.end()));   
                    }

                    /**
                     * @brief Copy and Merge
                     * @param moveVec Objects to be merged
                     * 
                     */
                    auto CopyMerge(const T& copyVec)
                    requires(!is_const){
                        return (*vec).insert((*vec).end(),
                            copyVec.begin(),
                            copyVec.end());   
                    }
                    /**
                     * @brief Copy and Merge
                     * @param moveVec Objects to be merged
                     * @param range Insertion Position
                     * 
                     */
                    auto CopyMerge(const T& copyVec ,VecIt range)
                    requires(!is_const){
                        return (*vec).insert(range,
                            copyVec.begin(),
                            copyVec.end());   
                    }
                
                    /**
                     * @brief Find objects that meet the specified criteria using a function.
                     * @param f Object lookup function
                     * 
                     * @note std::vector<int>(The index of the marked object)
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
            static OpAgent<std::vector<T>> control(std::vector<T>&& vec){
                return OpAgent<std::vector<T>>(std::move(vec));
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
            static auto sendIndex(std::vector<int> index){
                return [index = std::move(index)](auto* vec){
                    return [vec, index = std::move(index)](auto&& f){
                        for(auto& i : index) f((*vec)[i]);
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
    };




};

namespace OpVec{
    using vecUtils::Controller::control;
    using vecUtils::Controller::range;
    using vecUtils::Controller::sendAll;
    using vecUtils::Controller::sendIndex;
};
namespace FunWrap = vecUtils::FunWrap;


#endif //BETTER_CONTAINER