
# Currently we are in the Code folder: ${CMAKE_CURRENT_LIST_DIR}
# Get the platform specific folder ${pal_dir} for the current folder: ${CMAKE_CURRENT_LIST_DIR}/Platform/${PAL_PLATFORM_NAME}
# Note: o3de_pal_dir will take care of the details for us, as this may be a restricted platform
#       in which case it will see if that platform is present here or in the restricted folder.
#       i.e. It could here in our gem : Gems/LuaVSCode/Code/Platform/<platorm_name>  or
#            <restricted_folder>/<platform_name>/Gems/LuaVSCode/Code
o3de_pal_dir(pal_dir ${CMAKE_CURRENT_LIST_DIR}/Platform/${PAL_PLATFORM_NAME} "${gem_restricted_path}" "${gem_path}" "${gem_parent_relative_path}")

# Now that we have the platform abstraction layer (PAL) folder for this folder, thats where we will find the
# traits for this platform. Traits for a platform are defines for things like whether or not something in this gem
# is supported by this platform.
include(${pal_dir}/PAL_${PAL_PLATFORM_NAME_LOWERCASE}.cmake)

# Check to see if building the Gem Modules are supported for the current platform
if(NOT PAL_TRAIT_LUAVSCODE_SUPPORTED)
    return()
endif()

# set(CPPDAP_DIR <path-to-cppdap>) # example <path-to-cppdap>: "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppdap"
# add_subdirectory(${CPPDAP_DIR})

# The ${gem_name}.API target declares the common interface that users of this gem should depend on in their targets
ly_add_target(
    NAME ${gem_name}.API INTERFACE
    NAMESPACE Gem
    FILES_CMAKE
        luavscode_api_files.cmake
        ${pal_dir}/luavscode_api_files.cmake
    INCLUDE_DIRECTORIES
        INTERFACE
            Include
    BUILD_DEPENDENCIES
        INTERFACE
           AZ::AzCore
)

# The ${gem_name}.Private.Object target is an internal target
# It should not be used outside of this Gems CMakeLists.txt
ly_add_target(
    NAME ${gem_name}.Private.Object STATIC
    NAMESPACE Gem
    FILES_CMAKE
        luavscode_private_files.cmake
        ${pal_dir}/luavscode_private_files.cmake
    TARGET_PROPERTIES
        O3DE_PRIVATE_TARGET TRUE
    INCLUDE_DIRECTORIES
        PRIVATE
            Include
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            AZ::AzCore
            AZ::AzFramework
)

# Here add ${gem_name} target, it depends on the Private Object library and Public API interface
ly_add_target(
    NAME ${gem_name} ${PAL_TRAIT_MONOLITHIC_DRIVEN_MODULE_TYPE}
    NAMESPACE Gem
    FILES_CMAKE
        luavscode_shared_files.cmake
        ${pal_dir}/luavscode_shared_files.cmake
    INCLUDE_DIRECTORIES
        PUBLIC
            Include
        PRIVATE
            Source
    BUILD_DEPENDENCIES
        PUBLIC
            Gem::${gem_name}.API
        PRIVATE
            Gem::${gem_name}.Private.Object
)

# By default, we will specify that the above target ${gem_name} would be used by
# Client and Server type targets when this gem is enabled.  If you don't want it
# active in Clients or Servers by default, delete one of both of the following lines:
ly_create_alias(NAME ${gem_name}.Clients NAMESPACE Gem TARGETS Gem::${gem_name})
ly_create_alias(NAME ${gem_name}.Servers NAMESPACE Gem TARGETS Gem::${gem_name})

# For the Client and Server variants of ${gem_name} Gem, an alias to the ${gem_name}.API target will be made
ly_create_alias(NAME ${gem_name}.Clients.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)
ly_create_alias(NAME ${gem_name}.Servers.API NAMESPACE Gem TARGETS Gem::${gem_name}.API)

# If we are on a host platform, we want to add the host tools targets like the ${gem_name}.Editor MODULE target
if(PAL_TRAIT_BUILD_HOST_TOOLS)

    ly_add_target(
        NAME LuaVSCodeDebugAdapter EXECUTABLE
        NAMESPACE AZ
        FILES_CMAKE
            luavscode_debug_adapter_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                .
        BUILD_DEPENDENCIES
            PUBLIC
                Gem::RemoteTools
            PRIVATE
                AZ::AzCore
                AZ::AzFramework
                AZ::AzToolsFramework
                AZ::AzNetworking
                3rdParty::cppdap
    )


    # I do not know why ly_set_gem_variant_to_load does cause a .setreg to be generated
    # that specifies the gem dll to load.  Have to run ly_add_target_dependencies
    #ly_set_gem_variant_to_load(TARGETS LuaVSCodeDebugAdapter VARIANTS LuaTools)
    ly_add_target_dependencies("LuaVSCodeDebugAdapter" TARGETS LuaVSCodeDebugAdapter DEPENDENT_TARGETS Gem::RemoteTools)

    # Adds the PassCanvas target as a C preprocessor define so that it can be used as a Settings Registry
    # specialization in order to look up the generated .setreg which contains the dependencies
    # specified for the target.
    set_source_files_properties(
        Source/Tools/DebugAdapter/LUADebugAdapterApplication.cpp
        PROPERTIES
            COMPILE_DEFINITIONS
                LY_CMAKE_TARGET="LuaVSCodeDebugAdapter"
    )


    if(CMAKE_SYSTEM_NAME MATCHES "Windows")
        set(extroot "$ENV{USERPROFILE}\\.vscode\\extensions")
        set(O3DE_VSCODE_PACKAGE_PROGRAM "LuaVSCodeDebugAdapter.exe")
    else()
        set(extroot "$ENV{HOME}/.vscode/extensions")
        set(O3DE_VSCODE_PACKAGE_PROGRAM "LuaVSCodeDebugAdapter")
    endif()
    if(EXISTS ${extroot})

        set(extdir "${extroot}/o3de.luadebugadapter-1.0.0")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Source/Tools/DebugAdapter/package.json" ${extdir}/package.json)
        add_custom_command(TARGET LuaVSCodeDebugAdapter
           POST_BUILD
           COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:LuaVSCodeDebugAdapter> ${extdir})
    else()
        message(WARNING "Could not install lua debug adapter extension because '${extroot}' does not exist")
    endif()


    # The ${gem_name}.Editor.API target can be used by other gems that want to interact with the ${gem_name}.Editor module
    ly_add_target(
        NAME ${gem_name}.Editor.API INTERFACE
        NAMESPACE Gem
        FILES_CMAKE
            luavscode_editor_api_files.cmake
            ${pal_dir}/luavscode_editor_api_files.cmake
        INCLUDE_DIRECTORIES
            INTERFACE
                Include
        BUILD_DEPENDENCIES
            INTERFACE
                AZ::AzToolsFramework
    )

    # The ${gem_name}.Editor.Private.Object target is an internal target
    # which is only to be used by this gems CMakeLists.txt and any subdirectories
    # Other gems should not use this target
    ly_add_target(
        NAME ${gem_name}.Editor.Private.Object STATIC
        NAMESPACE Gem
        FILES_CMAKE
            luavscode_editor_private_files.cmake
        TARGET_PROPERTIES
            O3DE_PRIVATE_TARGET TRUE
        INCLUDE_DIRECTORIES
            PRIVATE
                Include
                Source
        BUILD_DEPENDENCIES
            PUBLIC
                AZ::AzToolsFramework
                $<TARGET_OBJECTS:Gem::${gem_name}.Private.Object>
    )

    ly_add_target(
        NAME ${gem_name}.Editor GEM_MODULE
        NAMESPACE Gem
        AUTOMOC
        FILES_CMAKE
            luavscode_editor_shared_files.cmake
        INCLUDE_DIRECTORIES
            PRIVATE
                Source
            PUBLIC
                Include
        BUILD_DEPENDENCIES
            PUBLIC
                Gem::${gem_name}.Editor.API
            PRIVATE
                Gem::${gem_name}.Editor.Private.Object
    )

    # By default, we will specify that the above target ${gem_name} would be used by
    # Tool and Builder type targets when this gem is enabled.  If you don't want it
    # active in Tools or Builders by default, delete one of both of the following lines:
    ly_create_alias(NAME ${gem_name}.Tools    NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)
    ly_create_alias(NAME ${gem_name}.Builders NAMESPACE Gem TARGETS Gem::${gem_name}.Editor)

    # For the Tools and Builders variants of ${gem_name} Gem, an alias to the ${gem_name}.Editor API target will be made
    ly_create_alias(NAME ${gem_name}.Tools.API NAMESPACE Gem TARGETS Gem::${gem_name}.Editor.API)
    ly_create_alias(NAME ${gem_name}.Builders.API NAMESPACE Gem TARGETS Gem::${gem_name}.Editor.API)

endif()

################################################################################
# Tests
################################################################################
# See if globally, tests are supported
if(PAL_TRAIT_BUILD_TESTS_SUPPORTED)
    # We globally support tests, see if we support tests on this platform for ${gem_name}.Tests
    if(PAL_TRAIT_LUAVSCODE_TEST_SUPPORTED)
        # We support ${gem_name}.Tests on this platform, add dependency on the Private Object target
        ly_add_target(
            NAME ${gem_name}.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
            NAMESPACE Gem
            FILES_CMAKE
                luavscode_tests_files.cmake
            INCLUDE_DIRECTORIES
                PRIVATE
                    Tests
                    Source
            BUILD_DEPENDENCIES
                PRIVATE
                    AZ::AzTest
                    AZ::AzFramework
                    Gem::${gem_name}.Private.Object
        )

        # Add ${gem_name}.Tests to googletest
        ly_add_googletest(
            NAME Gem::${gem_name}.Tests
        )
    endif()

    # If we are a host platform we want to add tools test like editor tests here
    if(PAL_TRAIT_BUILD_HOST_TOOLS)
        # We are a host platform, see if Editor tests are supported on this platform
        if(PAL_TRAIT_LUAVSCODE_EDITOR_TEST_SUPPORTED)
            # We support ${gem_name}.Editor.Tests on this platform, add ${gem_name}.Editor.Tests target which depends on
            # private ${gem_name}.Editor.Private.Object target
            ly_add_target(
                NAME ${gem_name}.Editor.Tests ${PAL_TRAIT_TEST_TARGET_TYPE}
                NAMESPACE Gem
                FILES_CMAKE
                    luavscode_editor_tests_files.cmake
                INCLUDE_DIRECTORIES
                    PRIVATE
                        Tests
                        Source
                BUILD_DEPENDENCIES
                    PRIVATE
                        AZ::AzTest
                        Gem::${gem_name}.Private.Object
            )

            # Add ${gem_name}.Editor.Tests to googletest
            ly_add_googletest(
                NAME Gem::${gem_name}.Editor.Tests
            )
        endif()
    endif()
endif()
