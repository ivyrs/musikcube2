set (subsonicsource_SOURCES
  subsonicsource_plugin.cpp
  SubsonicClient.cpp
  SubsonicIndexerSource.cpp
  Md5.cpp)

add_library(subsonicsource SHARED ${subsonicsource_SOURCES})

if (${BUILD_STANDALONE} MATCHES "true")
  include_directories(BEFORE ${VENDOR_INCLUDE_DIRECTORIES})
endif()

target_include_directories(subsonicsource BEFORE PUBLIC)
target_link_libraries(subsonicsource ${LIBCURL})
