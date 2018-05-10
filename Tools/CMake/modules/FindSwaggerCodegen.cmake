
find_package(Java COMPONENTS Runtime REQUIRED)
include(UseJava)

function(swagger_codegen API_NAME SPEC_FILE SRC_FOLDER TRG_FOLDER )
	set(swagger_codegen_jar "${SDK_DIR}/swagger-codegen/swagger-codegen-cli.jar")

	set(api_package         backend.${API_NAME}.api)
	set(model_package       backend.${API_NAME}.model)
	set(invoker_package     backend.core)
	
	set(timestamp_file      "${TRG_FOLDER}/${SPEC_FILE}.timestamp")

	file(TIMESTAMP "${SRC_FOLDER}/${SPEC_FILE}" src_timestamp UTC)
	file(TIMESTAMP "${timestamp_file}"          trg_timestamp UTC)

	message(STATUS "swagger_codegen: ${SRC_FOLDER}/${SPEC_FILE} timestamp = ${src_timestamp}")
	message(STATUS "swagger_codegen: ${timestamp_file} timestamp = ${trg_timestamp}")

	if (src_timestamp STRGREATER trg_timestamp)
		message(STATUS "swagger_codegen for: ${SPEC_FILE} in dir: ${TRG_FOLDER}")
		file(MAKE_DIRECTORY "${TRG_FOLDER}")
		file(WRITE "${timestamp_file}" "")

		execute_process(
			COMMAND ${Java_JAVA_EXECUTABLE} -jar ${swagger_codegen_jar} generate -i ${SRC_FOLDER}/${SPEC_FILE} -l cpprest --api-package ${api_package} --model-package ${model_package} --invoker-package ${invoker_package} -o ${TRG_FOLDER}
		)
	endif()
endfunction()
