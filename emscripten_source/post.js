// ENVIRONMENT
Module["ENVIRONMENT"] = ENVIRONMENT_IS_WEB ? "WEB" :
                        ENVIRONMENT_IS_WORKER ? "WORKER" :
                        ENVIRONMENT_IS_NODE ? "NODE" : "SHELL";

Module["getNativeTypeSize"] = getNativeTypeSize;

// export for browserify
if(!ENVIRONMENT_IS_NODE && typeof module !== 'undefined')
    module["exports"] = Module;
