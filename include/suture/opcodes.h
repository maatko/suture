#ifndef SUTURE_OPCODES_H
#define SUTURE_OPCODES_H

#define ACC_PUBLIC 0x0001       /**< Accessible from outside its package. */
#define ACC_PRIVATE 0x0002      /**< Accessible only within the defining class. */
#define ACC_PROTECTED 0x0004    /**< Accessible within the defining class and its subclasses. */
#define ACC_STATIC 0x0008       /**< Belongs to the class rather than any instance. */
#define ACC_FINAL 0x0010        /**< Must not be overridden by subclasses (JVMS §5.4.5). */
#define ACC_SYNCHRONIZED 0x0020 /**< Invocation is wrapped by a monitor acquire/release. */
#define ACC_BRIDGE 0x0040       /**< Compiler-generated bridge method for generics/covariance. */
#define ACC_VARARGS 0x0080      /**< Accepts a variable number of arguments. */
#define ACC_NATIVE 0x0100       /**< Implemented in a language other than Java. */
#define ACC_ABSTRACT 0x0400     /**< Declared abstract; no implementation provided. */
#define ACC_STRICT 0x0800       /**< FP-strict floating-point mode (@c strictfp). */
#define ACC_SYNTHETIC 0x1000    /**< Compiler-generated; not present in source code. */

#endif // SUTURE_OPCODES_H
