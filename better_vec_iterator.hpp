#ifndef BETTER_VEC_ITERATOR
#define BETTER_VEC_ITERATOR



#include <concepts>
#include <ranges>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>
#include <cstddef>
#include <tuple>
#include <memory>

namespace vectorIteratorExtension{
    namespace detail{

        template <class X> struct template_arg;

        template <template <class...> class Tmpl, class... Args>
        struct template_arg<Tmpl<Args...>> {
            using type = std::tuple_element_t<0, std::tuple<Args...>>;  // 取第一个
        };

        template <class X>
        using template_arg_t = typename template_arg<std::remove_cvref_t<X>>::type;



        template<std::input_iterator It>
        struct it_range{
            It begin, end;
        };   

        struct DefaultRange{
            template<std::ranges::range C, class F>
            void operator()(C& container, F&& f)const{
                for(auto& i : container) f(i);
            }
            template<std::input_iterator It, class F>
            void operator()(it_range<It> range, F&& f)const{
                for(auto i = range.begin; i != range.end;++i) f(*i);
            }
        };

        template<class F>
        struct ifRange{
            private:
                F conditionSelect;
            public:
                template<class FuncParams>
                    requires (!std::same_as<std::remove_cvref_t<FuncParams>, ifRange>)
                ifRange(FuncParams&& f):
                    conditionSelect(std::forward<FuncParams>(f)){}

                template<std::ranges::range C, class Func>
                void operator()(C& container, Func&& f)const{
                    for(auto& i : container) if(conditionSelect(i)) f(i);
                }
                template<std::input_iterator It, class Func>
                void operator()(it_range<It> range, Func&& f)const{
                    for(auto i = range.begin; i != range.end;++i) if(conditionSelect(*i)) f(*i);
                }
        };
        template<class F>
        ifRange(F&&) -> ifRange<std::decay_t<F>>;


        template<std::ranges::input_range R>
        struct indexRange_indexContainer{
            private:
                R* indexSet;
            public:
                explicit indexRange_indexContainer(R& i):indexSet(&i){}

                template<std::ranges::random_access_range C, class Func>
                void operator()(C& container, Func&& f)const{
                    for(auto& i : *indexSet) f(container[i]);
                }
                template<std::random_access_iterator It, class Func>
                void operator()(it_range<It> range, Func&& f)const{
                    for(auto& i : *indexSet) f(*(range.begin + i));
                }
        };

        template<std::convertible_to<std::size_t>... Rest>
        struct indexRange_explicit{
            private:
                std::tuple<std::decay_t<Rest>...> indexParams;
            public:
                explicit indexRange_explicit(Rest&&... i):indexParams(std::forward<Rest>(i)...){}

                template<std::ranges::random_access_range C, class Func>
                void operator()(C& container, Func&& f)const{
                    std::apply([&container, &f](auto&&... param){
                        (f(container[param]), ...);
                    }, indexParams);
                }
                template<std::random_access_iterator It, class Func>
                void operator()(it_range<It> range, Func&& f)const{
                    std::apply([&range, &f](auto&&... param){
                        (f(*(range.begin + param)), ...);
                    }, indexParams);
                }
        };

        template<class... Rest>
        indexRange_explicit(Rest&&...) -> indexRange_explicit<Rest...>;

        template<class It>
        concept vector_iterator =
            std::contiguous_iterator<std::remove_cvref_t<It>>
            && (std::same_as<std::remove_cvref_t<It>,
                    typename std::vector<std::iter_value_t<std::remove_cvref_t<It>>>::iterator>
            || std::same_as<std::remove_cvref_t<It>,
                    typename std::vector<std::iter_value_t<std::remove_cvref_t<It>>>::const_iterator>);
    };

    
    
    namespace funcCollect{
        struct PlaceholderT{};  //占位类型

        template<class dataT = PlaceholderT, class... Params>
        class pipeline{
            private:
                friend class Chain;

                std::tuple<Params...> flowList;
                static constexpr bool is_PlaceholderT = std::is_same_v<dataT, PlaceholderT>;

                template<class F>
                static constexpr bool chainCondition(){
                    if constexpr(is_PlaceholderT){
                        return true;
                    }else{
                        return requires{std::declval<F>()(std::declval<dataT>());};
                    } 
                }

                template<class retT, class F>
                auto make_next(F&& f){
                    return std::apply([&f]<class... Param>(Param&&... param){
                        return pipeline<retT, Params..., std::decay_t<F>>(
                            std::forward<Param>(param)...,
                            std::forward<F>(f));
                    }, std::move(flowList));
                }
            public:
                pipeline() = default;
                template<class... Ps> requires(sizeof...(Ps) == sizeof...(Params))
                explicit pipeline(Ps&&... ps) : flowList(std::forward<Ps>(ps)...) {}
                

                template<class Func>
                auto operator|(Func&& f)
                requires(chainCondition<Func>()){
                    if constexpr(is_PlaceholderT){
                        return make_next<PlaceholderT>(std::forward<Func>(f));
                    }else{
                        using funcReturnT = std::invoke_result_t<Func, dataT>;
                        return make_next<funcReturnT>(std::forward<Func>(f));
                    }
                }

        };
        class Chain{
            public:
            template<typename T, class... Params>
            static auto extract(const pipeline<T, Params...>& pipe){
                return pipe.flowList;
            }
            template<typename T, class... Params>
            static auto extract(pipeline<T, Params...>&& pipe){
                return std::move(pipe.flowList);
            }
        };
    };

    namespace funcFusion{

        template<class... dataT, class Func>
        decltype(auto) funcFusion_recursionHelper(std::tuple<dataT...>&& data, Func&& f){
            return std::apply([&f](auto&&... param) -> decltype(auto){
                return std::forward<Func>(f)(std::forward<decltype(param)>(param)...);
            }, std::move(data));
        }

        template<class... dataT, class Func, class... FuncParams>
        decltype(auto) funcFusion_recursionHelper(std::tuple<dataT...>&& data, Func&& f, FuncParams&&... param){

            static_assert(requires{ std::declval<Func&>()(std::declval<dataT>()...); },
                "The function return value is incompatible with the next function.");

            decltype(auto) r = std::apply([&f](auto&&... param) -> decltype(auto){
                return std::forward<Func>(f)(std::forward<decltype(param)>(param)...);
            }, std::move(data));

            return funcFusion_recursionHelper(
                std::forward_as_tuple(std::forward<decltype(r)>(r)),
                std::forward<FuncParams>(param)...);
        }

        template<class... funcList>
        auto Pack(funcList&&... fList){
            static_assert(sizeof...(funcList) > 0, "The parameter cannot be less than 0.");
            return [...fList = std::forward<funcList>(fList)]<class... dataT>(dataT&&... data)mutable->decltype(auto){
                return funcFusion_recursionHelper(std::forward_as_tuple(std::forward<dataT>(data)...), fList...);
            };
        }
    };

    namespace VecProcessor{

        template<typename T, typename dataT, typename Range = detail::DefaultRange>
        decltype(auto) makeHandler(T&& elementProcessor, dataT&& containerData, Range&& targetRange = Range{}){   

            using elementRefT = std::iter_reference_t<decltype(std::begin(containerData))>;

            static_assert(requires{ std::declval<T&>()(std::declval<elementRefT>()); },
                "The processor cannot accept this element reference type.");

            using processorReturnT = decltype(elementProcessor(std::declval<elementRefT>()));

            if constexpr (std::is_reference_v<processorReturnT> || std::is_void_v<processorReturnT>){
                targetRange(containerData, elementProcessor);
                return std::forward<dataT>(containerData);
            }else{
                auto& ep = elementProcessor;
                std::vector<processorReturnT> tmpContainer;
                if constexpr (std::ranges::sized_range<std::remove_cvref_t<dataT>>){
                    tmpContainer.reserve(std::ranges::size(containerData));
                }
                targetRange(containerData, [&tmpContainer, &ep]<typename PDT>(PDT&& element){
                    tmpContainer.push_back(ep(std::forward<PDT>(element)));
                });
                return tmpContainer;                        
            }
            
        }

        template<typename T, typename It, typename Range = detail::DefaultRange>
        decltype(auto) makeHandler(T&& originalProcessor, detail::it_range<It> it, Range&& targetRange = Range{}){
            
            using elementRefT = std::iter_reference_t<It>;

            static_assert(requires{ std::declval<T&>()(std::declval<elementRefT>()); },
                "The function cannot accept the iterator element type.");

            using processorReturnT = decltype(originalProcessor(std::declval<elementRefT>()));

            if constexpr (std::is_reference_v<processorReturnT> || std::is_void_v<processorReturnT>){
                targetRange(it, originalProcessor);
                return it;
            }else{
                auto& ep = originalProcessor;
                std::vector<processorReturnT> tmpContainer;
                if constexpr (std::forward_iterator<It>){
                    tmpContainer.reserve(std::size_t(std::distance(it.begin, it.end)));
                }
                targetRange(it, [&tmpContainer, &ep]<typename PDT>(PDT&& element){
                    tmpContainer.push_back(ep(std::forward<PDT>(element)));
                });
                return tmpContainer;                        
            }
        }
    };

    namespace pipelineParse{

        template<class F>
        auto single_packager(F&& processor){
            struct defaultRange{};            
            return [processor = std::forward<F>(processor)]   
                <class dataT,class rangeF = defaultRange>(dataT&& data, rangeF&& range = rangeF{})mutable -> decltype(auto){
                if constexpr (std::is_same_v<std::remove_cvref_t<rangeF>, defaultRange>){
                    return VecProcessor::makeHandler(processor, std::forward<dataT>(data));
                }else{
                    return VecProcessor::makeHandler(processor, std::forward<dataT>(data), std::forward<rangeF>(range));
                }
            };
        }
        template<class... Param>
        auto make_chain(Param&&... param){
            return std::make_tuple(single_packager(std::forward<Param>(param))...);
        }
        template<class... Param>
        auto chainParsePack(Param&&... param){
            return std::apply([]<class... chainP>(chainP&&... chain){
                return funcFusion::Pack(std::forward<chainP>(chain)...);
            }, make_chain(std::forward<Param>(param)...));
        }

    };


    namespace pipebegin{
        template<typename T = funcCollect::PlaceholderT>
        auto make_pipe(auto&& range){
            return funcCollect::pipeline<T, std::decay_t<decltype(range)>>(std::forward<decltype(range)>(range));
        }
    };
    //makeHandler
    namespace controller{

        template<typename dataT ,typename firstParam, typename... Param>
        decltype(auto) createRunner(dataT&& data,firstParam&& range, Param&&... chain){
            return pipelineParse::chainParsePack(std::forward<Param>(chain)...)
            (std::forward<dataT>(data), std::forward<firstParam>(range));
        }

        template<typename T, bool TAKEOVER = false>
        class controllerProxy_container{
            private:
                std::unique_ptr<T> owned = nullptr;
                T* vec;

                decltype(auto) getTarget(){
                    if constexpr(TAKEOVER){
                        return std::move(*vec);
                    }else{
                        return *vec;
                    }
                }

            public:
                controllerProxy_container(T* vec):
                    vec(vec){}
                controllerProxy_container(T&& vec):
                    owned(std::make_unique<T>(std::move(vec))),
                    vec(owned.get()){}

                    template<typename dataT, typename... Params>
                    decltype(auto) iterator(const funcCollect::pipeline<dataT, Params...>& pipe){
                        return std::apply([this]<class... Param>(Param&&... pipe_params)->decltype(auto){
                            return createRunner(getTarget(), std::forward<Param>(pipe_params)...);
                        }, funcCollect::Chain::extract(pipe));
                    }
        };
        template<typename T>
        class controllerProxy_range{
            private:
                detail::it_range<T> range;
            public: 
                controllerProxy_range(const detail::it_range<T>& r):
                    range(r){}

                    template<typename dataT, typename... Params>
                    decltype(auto) iterator(const funcCollect::pipeline<dataT, Params...>& pipe){
                        return std::apply([this]<class... Param>(Param&&... pipe_params)->decltype(auto){
                            return createRunner(range, std::forward<Param>(pipe_params)...);
                        }, funcCollect::Chain::extract(pipe));
                    }
        };

        template<typename T>
        auto useTarget(std::vector<T>& vec){
            return controllerProxy_container<std::vector<T>>(&vec);
        }
        template<typename T>
        auto useTarget(const std::vector<T>& vec){
            return controllerProxy_container<const std::vector<T>>(&vec);
        }
        template<typename T>
        auto useTarget(std::vector<T>&& vec){
            return controllerProxy_container<std::vector<T>, true>(std::move(vec));
        }
        template<std::input_iterator It>
        auto useTarget(const It& begin, const It& end)
        requires(detail::vector_iterator<It>){
            return controllerProxy_range<It>(detail::it_range<It>{begin,end});
        }
        template<std::input_iterator It>
        auto useTarget(const detail::it_range<It>& range)
        requires(detail::vector_iterator<It>){
            return controllerProxy_range<It>(range);
        }
    };



    namespace rangeSelector{
        auto All(){
            return detail::DefaultRange{};
        }
        template<class F> 
        auto If(F&& condition){
            return detail::ifRange{std::forward<F>(condition)};
        }
        template<std::ranges::input_range R>
        auto Index(R& container){
            return detail::indexRange_indexContainer{container};
        }
        template<std::convertible_to<std::size_t>... Rest>
        auto Index(Rest&&... index){
            return detail::indexRange_explicit<Rest...>{std::forward<Rest>(index)...};
        }
    };












};



/**
 * Export API
 * 
 */
namespace ItExt{
    namespace range{
        using namespace vectorIteratorExtension::rangeSelector;
    };
    namespace Tools{
        using vectorIteratorExtension::controller::createRunner;
        using vectorIteratorExtension::VecProcessor::makeHandler;
        using vectorIteratorExtension::pipebegin::make_pipe;
        template<class... Fs>
        auto fuse(Fs&&... fs){return vectorIteratorExtension::funcFusion::Pack(std::forward<Fs>(fs)...);}
    };
    using vectorIteratorExtension::controller::useTarget;
    using namespace vectorIteratorExtension::pipebegin;
    using vectorIteratorExtension::funcCollect::pipeline;
    using vectorIteratorExtension::funcCollect::Chain;
    using vectorIteratorExtension::funcFusion::Pack;
};




#endif  // BETTER_VEC_ITERATOR