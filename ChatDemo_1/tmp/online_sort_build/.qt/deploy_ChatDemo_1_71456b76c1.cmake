include("D:/qtcode/ChatDemo_1/tmp/online_sort_build/.qt/QtDeploySupport.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ChatDemo_1-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_I18N_CATALOGS "qtbase")

qt6_deploy_runtime_dependencies(
    EXECUTABLE "D:/qtcode/ChatDemo_1/tmp/online_sort_build/ChatDemo_1.exe"
    GENERATE_QT_CONF
)
