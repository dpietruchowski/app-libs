add_library(qmllive SHARED
 filewatcher.cpp
 filewatcher.h
)

target_link_libraries(qmllive PRIVATE Qt6::Core Qt6::Qml Qt6::Quick)