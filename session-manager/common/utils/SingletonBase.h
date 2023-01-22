/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Singleton pattern template
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Martin Haimberger <martin.haimberger@thincast.com>
 *
 * This file may be used under the terms of the GNU Affero General
 * Public License version 3 as published by the Free Software Foundation
 * and appearing in the file LICENSE-AGPL included in the distribution
 * of this file.
 *
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Core AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifndef OGON_SMGR_SINGLETONBASE_H_
#define OGON_SMGR_SINGLETONBASE_H_

/**
* @file	SingletonBase.h
*
* @brief	Singleton implementation using c++ templates.
*
* Singleton pattern:
*
* The singleton design pattern is a way of enforcing only one instance of an object. 
* This is achieved by making 3 fairly simple steps to a class. 
* Firstly making the constructor private, creating a static member variable that will contain the instance
* and then creating a static method for accessing the instance.
*/

#include <stdio.h>

/**
* @def	SINGLETON_ADD_INITIALISATION(className)
* 
* @brief	Adds protected constructor, destructor and copyconstructor for the class \<classNam\>. It also adds
*			the class <code>SingletonBase</code> as friendclass. This is necesary if using the singleton pattern with
*           templates, otherwise the singleton can not be constructed the first time.
*
*/

#define SINGLETON_ADD_INITIALISATION(className) friend class SingletonBase<className>; \
												protected:\
													className();\
													~className();\
													className( const className& );
/**
* @class	SingletonBase
*
* @brief	Singleton implementation using c++ templates.
*
* Singleton pattern:
*
* The singleton design pattern is a way of enforcing only one instance of an object.
* This is achieved by making 3 fairly simple steps to a class.
* Firstly making the constructor private, creating a static member variable that will contain the instance
* and then creating a static method for accessing the instance.
*
*
* To use this singleton template you have to do following steps:
* - Implment singleton using the template (ConnectionSingleton)
* - Use singleton
*
* Implment singleton using the template:
*
* @code
#define CONNECTION_SINGLETON ConnectionSingleton::instance()

class ConnectionSingleton : public SingletonBase<ConnectionSingleton> {

SINGLETON_ADD_INITIALISATION(ConnectionSingleton)

};
* @endcode
*
* Use singleton:
*
* @code
ConnectionSingleton::instance().foobar()

or

CONNECTION_SINGLETON.foobar()
* @endcode
*/
template <class Derived> class SingletonBase
{
public:

	/**
	* @fn	static Derived& instance()
	*
	* @brief	Getter of the Singelton. Returns the static instance of the class.
	*
	* @retval	Derived - One and only static instance of the class.
	*/

	static Derived& instance();

protected:

	/**
	* @fn	SingletonBase()
	*
	* @brief	Default protected constructor.
	*
	* The protected constructor prevents, that another instance is generated, unless the static one.
	* The constructor has to be protected instead of private, to allow applying it as template.
	*/

	SingletonBase() {
	}

	/**
	* @fn	~SingletonBase()
	*
	* @brief	Finaliser.
	*/

	~SingletonBase(){}

private:

	/**
	* @fn	SingletonBase( const SingletonBase& )
	*
	* @brief	private copy constructor.
	*
	* The copy constructor is in private scope, to prevent creation of another instance via copy constructor.
	*/

	SingletonBase( const SingletonBase& );
};


template <class Derived>
Derived& SingletonBase<Derived>::instance() {
	static Derived _instance;
	return _instance;
}

#endif /* OGON_SMGR_SINGLETONBASE_H_ */
