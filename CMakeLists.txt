cmake_minimum_required(VERSION 3.2.3)

#Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project("KashikaNativeLib")

# -- options ----------------------------------------------------
option(GLTF_BUILD_WITH_DRACO          "Build with Draco"               ON)

# ===============================================================

set(CMAKE_POSITION_INDEPENDENT_CODE On)

# -- draco setting ---------------------------------------------- 
if(GLTF_BUILD_WITH_DRACO)
    # DRACO setting
    add_subdirectory(externals/draco)

    ## beautify ME!
    set_target_properties(draco PROPERTIES FOLDER Draco)
    set_target_properties(draco_attributes PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_attributes_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_attributes_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_attributes_pred_schemes_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_attributes_pred_schemes_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_decode PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_encode PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_mesh_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_mesh_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_core PROPERTIES FOLDER Draco)
    set_target_properties(draco_core_bit_coders PROPERTIES FOLDER Draco)
    set_target_properties(draco_dec_config PROPERTIES FOLDER Draco)
    set_target_properties(draco_decoder PROPERTIES FOLDER Draco)
    set_target_properties(draco_enc_config PROPERTIES FOLDER Draco)
    set_target_properties(draco_encoder PROPERTIES FOLDER Draco)
    set_target_properties(draco_io PROPERTIES FOLDER Draco)
    set_target_properties(draco_mesh PROPERTIES FOLDER Draco)
    set_target_properties(draco_metadata PROPERTIES FOLDER Draco)
    set_target_properties(draco_metadata_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_metadata_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_point_cloud PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_point_cloud_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_point_cloud_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_points_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_points_enc PROPERTIES FOLDER Draco)
    set_target_properties(dracodec PROPERTIES FOLDER Draco)
    set_target_properties(dracoenc PROPERTIES FOLDER Draco)

    set(DRACO_LIB draco)
    add_definitions(-DENABLE_BUILD_WITH_DRACO)

    set(DRACO_SRC_PATH externals/draco/src/)
endif()


# -- compile options, include and libraries -----------------

set (CMAKE_CXX_STANDARD 11)

include_directories(
    ${CMAKE_BINARY_DIR}
    externals/draco/src/
    externals/glm/
    externals/
    src/
    ${BUILD_DIR}/
)

# -----------------------------------------------------------

# -- KIL lib ------------------------------------
add_library( kil STATIC
    ./src/kil/CopyTextureFile.cpp
    ./src/kil/CopyTextureFile_GdiPlus.cpp
    ./src/kil/CopyTextureFile_STB.cpp
    ./src/kil/ResizeTextureFile.cpp
    ./src/kil/HasAlphaChannel.cpp
)

# -- KML lib ------------------------------------
if(WIN32)
    set(Compatibility ./src/kml/Compatibility.cpp)
endif()

add_library( kml STATIC
    ./src/kml/Bound.cpp
    ./src/kml/CalculateBound.cpp
    ./src/kml/CalculateNormalsMesh.cpp
    ${Compatibility}
    ./src/kml/FlatIndicesMesh.cpp
    ./src/kml/GLTF2GLB.cpp
    ./src/kml/glTFExporter.cpp
    ./src/kml/Material.cpp
    ./src/kml/Mesh.cpp
    ./src/kml/Node.cpp
    ./src/kml/NodeExporter.cpp
    ./src/kml/Options.cpp
    ./src/kml/SaveToDraco.cpp
    ./src/kml/SplitNodeByMaterialID.cpp
    ./src/kml/Transform.cpp
    ./src/kml/TriangulateMesh.cpp
)

target_link_libraries( kml 
                       kil 
                       ${DRACO_LIB})
