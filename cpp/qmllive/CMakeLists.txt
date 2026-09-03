add_library(app_qmllive SHARED
 qmlsourceinterceptor.cpp
 qmlsourceinterceptor.h
)

target_link_libraries(app_qmllive PRIVATE Qt6::Core Qt6::Qml Qt6::Quick)
