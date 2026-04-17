#ifndef SUTURE_ERROR_H
#define SUTURE_ERROR_H

enum su_error {
  SU_OK = 0,

  // virtual machine specific
  SU_JVM_NO_VIRTUAL_MACHINES,
  SU_JVM_ATTACH_JNI_FAILURE,
  SU_JVM_ATTACH_JVMTI_FAILURE,
};

#endif // SUTURE_ERROR_H
