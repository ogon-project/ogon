# Based on FindFeature from the FreeRDP project
# https://github.com/FreeRDP/FreeRDP/blob/master/cmake/FindFeature.cmake

include(FeatureSummary)
macro(find_dependency _dependency _type _purpose _description _version)

	string(TOUPPER ${_dependency} _dependency_upper)
	string(TOLOWER ${_type} _type_lower)

	if(${_type} STREQUAL "DISABLED")
		set(_dependency_default "OFF")
		message(STATUS "Skipping ${_type_lower} dependency ${_dependency} for ${_purpose} (${_description})")
	else()
		if(${_type} STREQUAL "REQUIRED")
			set(_dependency_default "ON")
			message(STATUS "Finding ${_type_lower} dependency ${_dependency} (version \"${_version}\") for ${_purpose} (${_description})")
			find_package(${_dependency} ${_version} REQUIRED)
		elseif(${_type} STREQUAL "RECOMMENDED")
			if(NOT ${WITH_${_dependency_upper}})
				set(_dependency_default "OFF")
			  message(STATUS "Skipping ${_type_lower} dependency ${_dependency} (version \"${_version}\") for ${_purpose} (${_description})")
			else()
				set(_dependency_default "ON")
			  message(STATUS "Finding ${_type_lower} dependency ${_dependency} (version \"${_version}\") for ${_purpose} (${_description})")
				message(STATUS "    Disable dependency ${_dependency} using \"-DWITH_${_dependency_upper}=OFF\"")
				find_package(${_dependency} ${_version})
			endif()
		elseif(${_type} STREQUAL "OPTIONAL")
			if(${WITH_${_dependency_upper}})
				set(_dependency_default "ON")
				message(STATUS "Finding ${_type_lower} dependency ${_dependency} (version \"${_version}\") for ${_purpose} (${_description})")
				find_package(${_dependency} ${-version} REQUIRED)
			else()
				set(_dependency_default "OFF")
				message(STATUS "Skipping ${_type_lower} dependency ${_dependency} for ${_purpose} (${_description})")
				message(STATUS "    Enable dependency ${_dependency} using \"-DWITH_${_dependency_upper}=ON\"")
			endif()
		else()
			set(_dependency_default "ON")
			message(STATUS "Finding ${_type_lower} dependency ${_dependency} for ${_purpose} (${_description})")
			find_package(${_dependency})
		endif()
		

		if(NOT ${${_dependency_upper}_FOUND})
			if(${_dependency_default})
				message(WARNING "    dependency ${_dependency} was requested but could not be found! ${_dependency_default} / ${${_dependency_upper}_FOUND}")
			endif()
			set(_dependency_default "OFF")
		endif()

		option(WITH_${_dependency_upper} "Enable dependency ${_dependency} for ${_purpose}" ${_dependency_default})

		set_package_properties(${_dependency} PROPERTIES
			TYPE ${_type}
			PURPOSE "${_purpose}"
			DESCRIPTION "${_description}")
	endif()
endmacro(find_dependency)

