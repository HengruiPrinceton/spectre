// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "Parallel/ParallelComponentHelpers.hpp"
#include "Parallel/ReductionDeclare.hpp"
#include "Parallel/TypeTraits.hpp"
#include "Utilities/Functional.hpp"

/// \cond
namespace db {
template <typename TagsList>
class DataBox;
}  // namespace db
template <size_t MaxSize, class Key, class ValueType, class Hash,
          class KeyEqual>
class FixedHashMap;
namespace Parallel {
template <typename Metavariables>
class CProxy_MutableGlobalCache;
template <typename Metavariables>
class CkIndex_MutableGlobalCache;
template <typename Metavariables>
class MutableGlobalCache;
template <typename Metavariables>
class CProxy_GlobalCache;
template <typename Metavariables>
class CProxy_Main;
template <typename Metavariables>
class CkIndex_GlobalCache;
template <typename Metavariables>
class CkIndex_Main;
template <typename Metavariables>
class GlobalCache;
template <typename Metavariables>
class Main;
}  // namespace Parallel
/// \endcond

namespace Parallel {
namespace charmxx {
/*!
 * \ingroup CharmExtensionsGroup
 * \brief Class to mark a constructor as a constructor for the main chare.
 *
 * The main chare must have a constructor that takes a `const
 * Parallel::charmxx::MainChareRegistrationConstructor&` as its only argument.
 * This constructor is only used to trigger the `RegisterChare::registrar` code
 * needed for automatic registration.
 * \see RegisterChare
 */
struct MainChareRegistrationConstructor {};

/// \cond
struct RegistrationHelper;
extern std::unique_ptr<RegistrationHelper>* charm_register_list;
extern size_t charm_register_list_capacity;
extern size_t charm_register_list_size;
/// \endcond

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Returns the template parameter as a `std::string`
 *
 * Uses the __PRETTY_FUNCTION__ compiler intrinsic to extract the template
 * parameter names in the same form that Charm++ uses to register entry methods.
 * This is used by the generated Singleton, Array, Group and Nodegroup headers,
 * as well as in CharmMain.tpp.
 */
template <class... Args>
std::string get_template_parameters_as_string() {
  std::string function_name(static_cast<char const*>(__PRETTY_FUNCTION__));
  std::string template_params =
      function_name.substr(function_name.find(std::string("Args = ")) + 8);
  template_params.erase(template_params.end() - 2, template_params.end());
  size_t pos = 0;
  while ((pos = template_params.find(" >")) != std::string::npos) {
    template_params.replace(pos, 1, ">");
    template_params.erase(pos + 1, 1);
  }
  pos = 0;
  while ((pos = template_params.find(", ", pos)) != std::string::npos) {
    template_params.erase(pos + 1, 1);
  }
  pos = 0;
  while ((pos = template_params.find('>', pos + 2)) != std::string::npos) {
    template_params.replace(pos, 1, " >");
  }
  std::replace(template_params.begin(), template_params.end(), '%', '>');
  // GCC's __PRETTY_FUNCTION__ adds the return type at the end, so we remove it.
  if (template_params.find('}') != std::string::npos) {
    template_params.erase(template_params.find('}'), template_params.size());
  }
  // Remove all spaces
  const auto new_end =
      std::remove(template_params.begin(), template_params.end(), ' ');
  template_params.erase(new_end, template_params.end());
  return template_params;
}

/*!
 * \ingroup CharmExtensionsGroup
 * \brief The base class used for automatic registration of entry methods.
 *
 * Entry methods are automatically registered by building a list of the entry
 * methods that need to be registered in the `charm_register_list`. All entry
 * methods in the list are later registered in the
 * `register_parallel_components` function, at which point the list is also
 * deleted.
 *
 * The reason for using an abstract base class mechanism is that we need to be
 * able to register entry method templates. The derived classes keep track of
 * the template parameters and override the `register_with_charm` function.
 * The result is that there must be one derived class template for each entry
 * method template, but since we only have a few entry method templates this is
 * not an issue.
 */
struct RegistrationHelper {
  RegistrationHelper() = default;
  RegistrationHelper(const RegistrationHelper&) = default;
  RegistrationHelper& operator=(const RegistrationHelper&) = default;
  RegistrationHelper(RegistrationHelper&&) = default;
  RegistrationHelper& operator=(RegistrationHelper&&) = default;
  virtual ~RegistrationHelper() = default;

  virtual void register_with_charm() const = 0;
  virtual std::string name() const = 0;
  virtual bool is_registering_chare() const { return false; };
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering parallel components.
 *
 * Calls the appropriate Charm++ function to register a parallel component.
 */
template <typename ParallelComponent>
struct RegisterParallelComponent : RegistrationHelper {
  using chare_type = typename ParallelComponent::chare_type;
  using charm_type = charm_types_with_parameters<
      ParallelComponent,
      typename get_array_index<chare_type>::template f<ParallelComponent>>;
  using ckindex = typename charm_type::ckindex;
  using algorithm = typename charm_type::algorithm;

  RegisterParallelComponent() = default;
  RegisterParallelComponent(const RegisterParallelComponent&) = default;
  RegisterParallelComponent& operator=(const RegisterParallelComponent&) =
      default;
  RegisterParallelComponent(RegisterParallelComponent&&) = default;
  RegisterParallelComponent& operator=(RegisterParallelComponent&&) = default;
  ~RegisterParallelComponent() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    static const std::string parallel_component_name = name();
    ckindex::__register(parallel_component_name.c_str(), sizeof(algorithm));
  }

  bool is_registering_chare() const override { return true; }

  std::string name() const override {
    return get_template_parameters_as_string<algorithm>();
  }

  static bool registrar;
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering chares.
 *
 * Calls the appropriate Charm++ function to register a chare
 *
 * The chare that is being registered must have the following in the destructor:
 * \code
 * (void)Parallel::charmxx::RegisterChare<ChareName,
 *     CkIndex_ChareName>::registrar;
 * \endcode
 *
 * The main chare must also have a constructor that takes a `const
 * Parallel::charmxx::MainChareRegistrationConstructor&` as its only argument.
 * This constructor is only used to trigger the `RegisterChare::registrar` code
 * needed for automatic registration. The main chare is determined by specifying
 * the type alias `charmxx_main_component` before the `Parallel/CharmMain.tpp`
 * include.
 * \snippet Test_AlgorithmCore.cpp charm_main_example
 */
template <typename Chare, typename CkIndex>
struct RegisterChare : RegistrationHelper {
  RegisterChare() = default;
  RegisterChare(const RegisterChare&) = default;
  RegisterChare& operator=(const RegisterChare&) = default;
  RegisterChare(RegisterChare&&) = default;
  RegisterChare& operator=(RegisterChare&&) = default;
  ~RegisterChare() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    static const std::string chare_name = name();
    CkIndex::__register(chare_name.c_str(), sizeof(Chare));
  }

  bool is_registering_chare() const override { return true; }

  std::string name() const override {
    return get_template_parameters_as_string<Chare>();
  }

  static bool registrar;
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering simple actions.
 *
 * Calls the appropriate Charm++ function to register a simple action.
 */
template <typename ParallelComponent, typename Action, typename... Args>
struct RegisterSimpleAction : RegistrationHelper {
  using chare_type = typename ParallelComponent::chare_type;
  using charm_type = charm_types_with_parameters<
      ParallelComponent,
      typename get_array_index<chare_type>::template f<ParallelComponent>>;
  using cproxy = typename charm_type::cproxy;
  using ckindex = typename charm_type::ckindex;
  using algorithm = typename charm_type::algorithm;

  RegisterSimpleAction() = default;
  RegisterSimpleAction(const RegisterSimpleAction&) = default;
  RegisterSimpleAction& operator=(const RegisterSimpleAction&) = default;
  RegisterSimpleAction(RegisterSimpleAction&&) = default;
  RegisterSimpleAction& operator=(RegisterSimpleAction&&) = default;
  ~RegisterSimpleAction() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_simple_action<Action>(
        static_cast<void (algorithm::*)(const std::tuple<Args...>&)>(nullptr));
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterSimpleAction>();
  }

  static bool registrar;
};

/// \cond
template <typename ParallelComponent, typename Action>
struct RegisterSimpleAction<ParallelComponent, Action> : RegistrationHelper {
  using chare_type = typename ParallelComponent::chare_type;
  using charm_type = charm_types_with_parameters<
      ParallelComponent,
      typename get_array_index<chare_type>::template f<ParallelComponent>>;
  using cproxy = typename charm_type::cproxy;
  using ckindex = typename charm_type::ckindex;
  using algorithm = typename charm_type::algorithm;

  RegisterSimpleAction() = default;
  RegisterSimpleAction(const RegisterSimpleAction&) = default;
  RegisterSimpleAction& operator=(const RegisterSimpleAction&) = default;
  RegisterSimpleAction(RegisterSimpleAction&&) = default;
  RegisterSimpleAction& operator=(RegisterSimpleAction&&) = default;
  ~RegisterSimpleAction() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_simple_action<Action>(
        static_cast<void (algorithm::*)()>(nullptr));
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterSimpleAction>();
  }

  static bool registrar;
};
/// \endcond

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering threaded actions.
 *
 * Calls the appropriate Charm++ function to register a threaded action.
 */
template <typename ParallelComponent, typename Action, typename... Args>
struct RegisterThreadedAction : RegistrationHelper {
  using chare_type = typename ParallelComponent::chare_type;
  using charm_type = charm_types_with_parameters<
      ParallelComponent,
      typename get_array_index<chare_type>::template f<ParallelComponent>>;
  using cproxy = typename charm_type::cproxy;
  using ckindex = typename charm_type::ckindex;
  using algorithm = typename charm_type::algorithm;

  RegisterThreadedAction() = default;
  RegisterThreadedAction(const RegisterThreadedAction&) = default;
  RegisterThreadedAction& operator=(const RegisterThreadedAction&) = default;
  RegisterThreadedAction(RegisterThreadedAction&&) = default;
  RegisterThreadedAction& operator=(RegisterThreadedAction&&) = default;
  ~RegisterThreadedAction() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_threaded_action<Action>(
        static_cast<void (algorithm::*)(const std::tuple<Args...>&)>(nullptr));
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterThreadedAction>();
  }

  static bool registrar;
};

/// \cond
template <typename ParallelComponent, typename Action>
struct RegisterThreadedAction<ParallelComponent, Action> : RegistrationHelper {
  using chare_type = typename ParallelComponent::chare_type;
  using charm_type = charm_types_with_parameters<
      ParallelComponent,
      typename get_array_index<chare_type>::template f<ParallelComponent>>;
  using cproxy = typename charm_type::cproxy;
  using ckindex = typename charm_type::ckindex;
  using algorithm = typename charm_type::algorithm;

  RegisterThreadedAction() = default;
  RegisterThreadedAction(const RegisterThreadedAction&) = default;
  RegisterThreadedAction& operator=(const RegisterThreadedAction&) = default;
  RegisterThreadedAction(RegisterThreadedAction&&) = default;
  RegisterThreadedAction& operator=(RegisterThreadedAction&&) = default;
  ~RegisterThreadedAction() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_threaded_action<Action>(
        static_cast<void (algorithm::*)()>(nullptr));
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterThreadedAction>();
  }

  static bool registrar;
};
/// \endcond

namespace detail {
template <class T>
struct get_value_type {
  using type = T;
};

template <class T>
struct get_value_type<std::vector<T>> {
  using type = T;
};

template <class Key, class Mapped, class Hash, class KeyEqual, class Allocator>
struct get_value_type<
    std::unordered_map<Key, Mapped, Hash, KeyEqual, Allocator>> {
  // When sending data it is typical to use `std::make_pair(a, b)` which results
  // in a non-const Key type, which is different from what
  // `unordered_map::value_type` is (e.g. `std::pair<const Key, Mapped>`). This
  // difference leads to issues with function registration with Charm++ because
  // when registering `receive_data` methods we register the `inbox_type`'s
  // `value_type` (`std::pair<const Key, Mapped>` in this case), not the type
  // passed to `receive_data`.
  using type = std::pair<Key, Mapped>;
};

template <size_t Size, class Key, class Mapped, class Hash, class KeyEqual>
struct get_value_type<FixedHashMap<Size, Key, Mapped, Hash, KeyEqual>> {
  // When sending data it is typical to use `std::make_pair(a, b)` which results
  // in a non-const Key type, which is different from what
  // `FixedHashMap::value_type` is (e.g. `std::pair<const Key, Mapped>`). This
  // difference leads to issues with function registration with Charm++ because
  // when registering `receive_data` methods we register the `inbox_type`'s
  // `value_type` (`std::pair<const Key, Mapped>` in this case), not the type
  // passed to `receive_data`.
  using type = std::pair<Key, Mapped>;
};

template <class Key, class Hash, class KeyEqual, class Allocator>
struct get_value_type<std::unordered_multiset<Key, Hash, KeyEqual, Allocator>> {
  using type = Key;
};

template <class T>
using get_value_type_t = typename get_value_type<T>::type;
}  // namespace detail

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering receive_data functions
 *
 * Calls the appropriate Charm++ function to register a receive_data function.
 * There is a bug in Charm++ that doesn't allow default values for entry method
 * arguments for groups and nodegroups, so we have to handle the (node)group
 * cases separately from the singleton and array cases.
 */
template <typename ParallelComponent, typename ReceiveTag>
struct RegisterReceiveData : RegistrationHelper {
  using chare_type = typename ParallelComponent::chare_type;
  using charm_type = charm_types_with_parameters<
      ParallelComponent,
      typename get_array_index<chare_type>::template f<ParallelComponent>>;
  using cproxy = typename charm_type::cproxy;
  using ckindex = typename charm_type::ckindex;
  using algorithm = typename charm_type::algorithm;

  RegisterReceiveData() = default;
  RegisterReceiveData(const RegisterReceiveData&) = default;
  RegisterReceiveData& operator=(const RegisterReceiveData&) = default;
  RegisterReceiveData(RegisterReceiveData&&) = default;
  RegisterReceiveData& operator=(RegisterReceiveData&&) = default;
  ~RegisterReceiveData() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_receive_data<ReceiveTag>(
        static_cast<void (algorithm::*)(
            const typename ReceiveTag::temporal_id&,
            const detail::get_value_type_t<
                typename ReceiveTag::type::mapped_type>&,
            bool)>(nullptr));
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterReceiveData>();
  }

  static bool registrar;
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering reduction actions
 *
 * Calls the appropriate Charm++ functions to register a reduction action.
 */
template <typename ParallelComponent, typename Action, typename ReductionType>
struct RegisterReductionAction : RegistrationHelper {
  using chare_type = typename ParallelComponent::chare_type;
  using charm_type = charm_types_with_parameters<
      ParallelComponent,
      typename get_array_index<chare_type>::template f<ParallelComponent>>;
  using cproxy = typename charm_type::cproxy;
  using ckindex = typename charm_type::ckindex;
  using algorithm = typename charm_type::algorithm;

  RegisterReductionAction() = default;
  RegisterReductionAction(const RegisterReductionAction&) = default;
  RegisterReductionAction& operator=(const RegisterReductionAction&) = default;
  RegisterReductionAction(RegisterReductionAction&&) = default;
  RegisterReductionAction& operator=(RegisterReductionAction&&) = default;
  ~RegisterReductionAction() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_reduction_action<Action, ReductionType>(
        static_cast<void (algorithm::*)(const ReductionType&)>(nullptr));
    ckindex::template redn_wrapper_reduction_action<Action, ReductionType>(
        nullptr);
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterReductionAction>();
  }

  static bool registrar;
};

template <typename Metavariables, typename InvokeCombine, typename... Tags>
struct RegisterPhaseChangeReduction : RegistrationHelper {
  using cproxy = CProxy_Main<Metavariables>;
  using ckindex = CkIndex_Main<Metavariables>;
  using algorithm = Main<Metavariables>;

  RegisterPhaseChangeReduction() = default;
  RegisterPhaseChangeReduction(const RegisterPhaseChangeReduction&) = default;
  RegisterPhaseChangeReduction& operator=(const RegisterPhaseChangeReduction&) =
      default;
  RegisterPhaseChangeReduction(RegisterPhaseChangeReduction&&) = default;
  RegisterPhaseChangeReduction& operator=(RegisterPhaseChangeReduction&&) =
      default;
  ~RegisterPhaseChangeReduction() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_phase_change_reduction<InvokeCombine, Tags...>(
        static_cast<void (algorithm::*)(
            const ReductionData<
                ReductionDatum<tuples::TaggedTuple<Tags...>, InvokeCombine,
                               funcl::Identity, std::index_sequence<>>>&)>(
            nullptr));
    ckindex::template redn_wrapper_phase_change_reduction<InvokeCombine,
                                                          Tags...>(nullptr);
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterPhaseChangeReduction>();
  }

  static bool registrar;
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering MutableGlobalCache::mutate
 *
 * Calls the appropriate Charm++ function to register the mutate function.
 */
template <typename Metavariables, typename GlobalCacheTag, typename Function,
          typename... Args>
struct RegisterMutableGlobalCacheMutate : RegistrationHelper {
  using cproxy = CProxy_MutableGlobalCache<Metavariables>;
  using ckindex = CkIndex_MutableGlobalCache<Metavariables>;
  using algorithm = MutableGlobalCache<Metavariables>;

  RegisterMutableGlobalCacheMutate() = default;
  RegisterMutableGlobalCacheMutate(const RegisterMutableGlobalCacheMutate&) =
      default;
  RegisterMutableGlobalCacheMutate& operator=(
      const RegisterMutableGlobalCacheMutate&) = default;
  RegisterMutableGlobalCacheMutate(RegisterMutableGlobalCacheMutate&&) =
      default;
  RegisterMutableGlobalCacheMutate& operator=(
      RegisterMutableGlobalCacheMutate&&) = default;
  ~RegisterMutableGlobalCacheMutate() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_mutate<GlobalCacheTag, Function>(
        static_cast<void (algorithm::*)(const std::tuple<Args...>&)>(nullptr));
  }

  std::string name() const override {
    return get_template_parameters_as_string<
        RegisterMutableGlobalCacheMutate>();
  }

  static bool registrar;
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering GlobalCache::mutate
 *
 * Calls the appropriate Charm++ function to register the mutate function.
 */
template <typename Metavariables, typename GlobalCacheTag, typename Function,
          typename... Args>
struct RegisterGlobalCacheMutate : RegistrationHelper {
  using cproxy = CProxy_GlobalCache<Metavariables>;
  using ckindex = CkIndex_GlobalCache<Metavariables>;
  using algorithm = GlobalCache<Metavariables>;

  RegisterGlobalCacheMutate() = default;
  RegisterGlobalCacheMutate(const RegisterGlobalCacheMutate&) = default;
  RegisterGlobalCacheMutate& operator=(const RegisterGlobalCacheMutate&) =
      default;
  RegisterGlobalCacheMutate(RegisterGlobalCacheMutate&&) = default;
  RegisterGlobalCacheMutate& operator=(RegisterGlobalCacheMutate&&) = default;
  ~RegisterGlobalCacheMutate() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_mutate<GlobalCacheTag, Function>(
        static_cast<void (algorithm::*)(const std::tuple<Args...>&)>(nullptr));
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterGlobalCacheMutate>();
  }

  static bool registrar;
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Derived class for registering the invoke_iterable_action entry method.
 * This is a `local` entry method that is only used for tracing the code with
 * Charm++'s Projections.
 *
 * Calls the appropriate Charm++ function to register the invoke_iterable_action
 * function.
 */
template <typename ParallelComponent, typename Action, typename PhaseIndex,
          typename DataBoxIndex>
struct RegisterInvokeIterableAction : RegistrationHelper {
  using chare_type = typename ParallelComponent::chare_type;
  using charm_type = charm_types_with_parameters<
      ParallelComponent,
      typename get_array_index<chare_type>::template f<ParallelComponent>>;
  using cproxy = typename charm_type::cproxy;
  using ckindex = typename charm_type::ckindex;
  using algorithm = typename charm_type::algorithm;

  RegisterInvokeIterableAction() = default;
  RegisterInvokeIterableAction(const RegisterInvokeIterableAction&) = default;
  RegisterInvokeIterableAction& operator=(const RegisterInvokeIterableAction&) =
      default;
  RegisterInvokeIterableAction(RegisterInvokeIterableAction&&) = default;
  RegisterInvokeIterableAction& operator=(RegisterInvokeIterableAction&&) =
      default;
  ~RegisterInvokeIterableAction() override = default;

  void register_with_charm() const override {
    static bool done_registration{false};
    if (done_registration) {
      return;  // LCOV_EXCL_LINE
    }
    done_registration = true;
    ckindex::template idx_invoke_iterable_action<Action, PhaseIndex,
                                                 DataBoxIndex>(
        static_cast<bool (algorithm::*)()>(nullptr));
  }

  std::string name() const override {
    return get_template_parameters_as_string<RegisterInvokeIterableAction>();
  }

  static bool registrar;
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Function that adds a pointer to a specific derived class to the
 * `charm_register_list`
 *
 * Used to initialize the `registrar` bool of derived classes of
 * `RegistrationHelper`. When the function is invoked it appends the derived
 * class to the `charm_register_list`.
 *
 * \note The reason for not using a `std::vector` is that this did not behave
 * correctly when calling `push_back`. Specifically, the final vector was always
 * size 1, even though multiple elements were pushed back. The reason for that
 * behavior was never tracked down and so in the future it could be possible to
 * use a `std::vector`.
 */
template <typename Derived>
bool register_func_with_charm() {
  if (charm_register_list_size >= charm_register_list_capacity) {
    // clang-tidy: use gsl::owner (we don't use raw owning pointers unless
    // necessary)
    auto* const t =  // NOLINT
        new std::unique_ptr<RegistrationHelper>[charm_register_list_capacity +
                                                10];
    for (size_t i = 0; i < charm_register_list_capacity; ++i) {
      // clang-tidy: do not use pointer arithmetic
      t[i] = std::move(charm_register_list[i]);  // NOLINT
    }
    // clang-tidy: use gsl::owner (we don't use raw owning pointers unless
    // necessary)
    delete[] charm_register_list;  // NOLINT
    charm_register_list = t;
    charm_register_list_capacity += 10;
  }
  charm_register_list_size++;
  // clang-tidy: do not use pointer arithmetic
  charm_register_list[charm_register_list_size - 1] =  // NOLINT
      std::make_unique<Derived>();
  return true;
}
}  // namespace charmxx
}  // namespace Parallel

// clang-tidy: redundant declaration
template <typename ParallelComponent>
bool Parallel::charmxx::RegisterParallelComponent<
    ParallelComponent>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterParallelComponent<ParallelComponent>>();

// clang-tidy: redundant declaration
template <typename Chare, typename CkIndex>
bool Parallel::charmxx::RegisterChare<Chare, CkIndex>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterChare<Chare, CkIndex>>();

// clang-tidy: redundant declaration
template <typename ParallelComponent, typename Action, typename... Args>
bool Parallel::charmxx::RegisterSimpleAction<ParallelComponent, Action,
                                             Args...>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterSimpleAction<ParallelComponent, Action, Args...>>();

// clang-tidy: redundant declaration
template <typename ParallelComponent, typename Action>
bool Parallel::charmxx::RegisterSimpleAction<ParallelComponent,
                                             Action>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterSimpleAction<ParallelComponent, Action>>();

// clang-tidy: redundant declaration
template <typename ParallelComponent, typename Action, typename... Args>
bool Parallel::charmxx::RegisterThreadedAction<ParallelComponent, Action,
                                               Args...>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterThreadedAction<ParallelComponent, Action, Args...>>();

// clang-tidy: redundant declaration
template <typename ParallelComponent, typename Action>
bool Parallel::charmxx::RegisterThreadedAction<ParallelComponent,
                                               Action>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterThreadedAction<ParallelComponent, Action>>();

// clang-tidy: redundant declaration
template <typename ParallelComponent, typename ReceiveTag>
bool Parallel::charmxx::RegisterReceiveData<ParallelComponent,
                                            ReceiveTag>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterReceiveData<ParallelComponent, ReceiveTag>>();

// clang-tidy: redundant declaration
template <typename ParallelComponent, typename Action, typename ReductionType>
bool Parallel::charmxx::RegisterReductionAction<
    ParallelComponent, Action, ReductionType>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterReductionAction<ParallelComponent, Action, ReductionType>>();

// clang-tidy: redundant declaration
template <typename Metavariables, typename Invokable, typename... Tags>
bool Parallel::charmxx::RegisterPhaseChangeReduction<
    Metavariables, Invokable, Tags...>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterPhaseChangeReduction<Metavariables, Invokable, Tags...>>();

// clang-tidy: redundant declaration
template <typename Metavariables, typename GlobalCacheTag, typename Function,
          typename... Args>
bool Parallel::charmxx::RegisterMutableGlobalCacheMutate<
    Metavariables, GlobalCacheTag, Function,
    Args...>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<
        RegisterMutableGlobalCacheMutate<Metavariables, GlobalCacheTag,
                                         Function, Args...>>();

// clang-tidy: redundant declaration
template <typename Metavariables, typename GlobalCacheTag, typename Function,
          typename... Args>
bool Parallel::charmxx::RegisterGlobalCacheMutate<
    Metavariables, GlobalCacheTag, Function,
    Args...>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<RegisterGlobalCacheMutate<
        Metavariables, GlobalCacheTag, Function, Args...>>();

// clang-tidy: redundant declaration
template <typename ParallelComponent, typename Action, typename PhaseIndex,
          typename DataBoxIndex>
bool Parallel::charmxx::RegisterInvokeIterableAction<
    ParallelComponent, Action, PhaseIndex, DataBoxIndex>::registrar =  // NOLINT
    Parallel::charmxx::register_func_with_charm<RegisterInvokeIterableAction<
        ParallelComponent, Action, PhaseIndex, DataBoxIndex>>();

/// \cond
class CkReductionMsg;
/// \endcond

namespace Parallel {
namespace charmxx {
/*!
 * \ingroup CharmExtensionsGroup
 * \brief The type of a function pointer to a Charm++ custom reduction function.
 */
using ReducerFunctions = CkReductionMsg* (*)(int, CkReductionMsg**);
/// \cond
extern ReducerFunctions* charm_reducer_functions_list;
extern size_t charm_reducer_functions_capacity;
extern size_t charm_reducer_functions_size;
extern std::unordered_map<size_t, CkReduction::reducerType>
    charm_reducer_functions;
/// \endcond

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Class used for registering custom reduction function
 *
 * The static member variable is initialized before main is entered. This means
 * we are able to inject code into the beginning of the execution of an
 * executable by "using" the variable (casting to void counts) the function that
 * initializes the variable is called. We "use" `registrar` inside the
 * `Parallel::contribute_to_reduction` function.
 */
template <ReducerFunctions>
struct RegisterReducerFunction {
  static bool registrar;
};

/*!
 * \ingroup CharmExtensionsGroup
 * \brief Function that stores a function pointer to the custom reduction
 * function to be registered later.
 *
 * Used to initialize the `registrar` bool of `RegisterReducerFunction`. When
 * invoked it adds the function `F` of type `ReducerFunctions` to the list
 * `charm_reducer_functions_list`.
 *
 * \note The reason for not using a `std::vector<ReducerFunctions>` is that this
 * did not behave correctly when calling `push_back`. Specifically, the final
 * vector was always size 1, even though multiple elements were pushed back. The
 * reason for that behavior was never tracked down and so in the future it could
 * be possible to use a `std::vector`.
 */
template <ReducerFunctions F>
bool register_reducer_function() {
  if (charm_reducer_functions_size >= charm_reducer_functions_capacity) {
    // clang-tidy: use gsl::owner (we don't use raw owning pointers unless
    // necessary)
    auto* const t =  // NOLINT
        new ReducerFunctions[charm_reducer_functions_capacity + 10];
    for (size_t i = 0; i < charm_reducer_functions_capacity; ++i) {
      // clang-tidy: do not use pointer arithmetic
      t[i] = std::move(charm_reducer_functions_list[i]);  // NOLINT
    }
    // clang-tidy: use gsl::owner (we don't use raw owning pointers unless
    // necessary)
    delete[] charm_reducer_functions_list;  // NOLINT
    charm_reducer_functions_list = t;
    charm_reducer_functions_capacity += 10;
  }
  charm_reducer_functions_size++;
  // clang-tidy: do not use pointer arithmetic
  charm_reducer_functions_list[charm_reducer_functions_size - 1] = F;  // NOLINT
  return true;
}
}  // namespace charmxx
}  // namespace Parallel

// clang-tidy: do not use pointer arithmetic
template <Parallel::charmxx::ReducerFunctions F>
bool Parallel::charmxx::RegisterReducerFunction<F>::registrar =  // NOLINT
    Parallel::charmxx::register_reducer_function<F>();
