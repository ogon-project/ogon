# GetGitRevisionDescription requires FindGit which was added in version 2.8.2
# build won't fail but GIT_REVISION is set to n/a
if(${CMAKE_VERSION} VERSION_LESS 2.8.2)
  message(WARNING "GetGitRevisionDescription reqires (FindGit) cmake >= 2.8.2 to work properly -
	GIT_REVISION will be set to n/a")
endif()
