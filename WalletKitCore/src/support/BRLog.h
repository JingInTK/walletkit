/*
 * BRLog.h
 *
 *  Created on: Jul. 28, 2021
 *      Author: bryangoring
 */

#ifndef EXTERNAL_WALLETKIT_WALLETKITCORE_SRC_SUPPORT_BRLOG_H_
#define EXTERNAL_WALLETKIT_WALLETKITCORE_SRC_SUPPORT_BRLOG_H_


#ifdef __EXPOSE_EVENTS__
#define uni_log(tag, ...) _uni_log("%s: " _va_first_(__VA_ARGS__, NULL) "\n", tag,\
                                      _va_rest_(__VA_ARGS__, NULL))
#define _va_first_(first, ...) first
#define _va_rest_(first, ...) __VA_ARGS__

#if defined(TARGET_OS_MAC)
#include <Foundation/Foundation.h>
#define _uni_log(...) NSLog(__VA_ARGS__)
#elif defined(__ANDROID__)
#include <android/log.h>
#define _uni_log(...) __android_log_print(ANDROID_LOG_INFO, "bread", __VA_ARGS__)
#else
#include <stdio.h>
#define _uni_log(...) printf(__VA_ARGS__)
#endif
#else //

#define uni_log(tag, ...)

#endif // __EXPOSE_EVENTS__
#endif /* EXTERNAL_WALLETKIT_WALLETKITCORE_SRC_SUPPORT_BRLOG_H_ */
