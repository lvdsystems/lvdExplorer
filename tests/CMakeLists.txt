find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(searchenginetest searchenginetest.cpp)
target_link_libraries(searchenginetest PRIVATE lvdCore Qt6::Test)
add_test(NAME SearchEngineTest COMMAND searchenginetest)

qt_add_executable(opsenginetest opsenginetest.cpp)
target_link_libraries(opsenginetest PRIVATE lvdCore Qt6::Test)
add_test(NAME OpsEngineTest COMMAND opsenginetest)

qt_add_executable(foldercomparetest foldercomparetest.cpp)
target_link_libraries(foldercomparetest PRIVATE lvdCore Qt6::Test)
add_test(NAME FolderCompareTest COMMAND foldercomparetest)

qt_add_executable(shortcutmanagertest shortcutmanagertest.cpp)
target_link_libraries(shortcutmanagertest PRIVATE lvdUi Qt6::Test)
add_test(NAME ShortcutManagerTest COMMAND shortcutmanagertest)

qt_add_executable(fileclipboardtest fileclipboardtest.cpp)
target_link_libraries(fileclipboardtest PRIVATE lvdUi Qt6::Test)
add_test(NAME FileClipboardTest COMMAND fileclipboardtest)

qt_add_executable(contentsearchtest contentsearchtest.cpp)
target_link_libraries(contentsearchtest PRIVATE lvdCore Qt6::Test)
add_test(NAME ContentSearchTest COMMAND contentsearchtest)

qt_add_executable(filesystemmodeltest filesystemmodeltest.cpp)
target_link_libraries(filesystemmodeltest PRIVATE lvdCore Qt6::Test)
add_test(NAME FileSystemModelTest COMMAND filesystemmodeltest)

qt_add_executable(drivelisttasktest drivelisttasktest.cpp)
target_link_libraries(drivelisttasktest PRIVATE lvdCore Qt6::Test)
add_test(NAME DriveListTaskTest COMMAND drivelisttasktest)

qt_add_executable(checksumtasktest checksumtasktest.cpp)
target_link_libraries(checksumtasktest PRIVATE lvdCore Qt6::Test)
add_test(NAME ChecksumTaskTest COMMAND checksumtasktest)
