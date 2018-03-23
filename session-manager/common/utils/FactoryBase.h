/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Factory pattern template
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

#ifndef _OGON_SMGR_FACTORYBASE_H_
#define _OGON_SMGR_FACTORYBASE_H_


/**
* @file FactoryBase.h
*
* @brief Factory implementation class using c++ templates.
*
* This class implements the Factory pattern. Therefor c++ templating was used.
*
* Factory pattern: 
*
* Factory Method is a creational pattern. This pattern helps to model an interface for creating an object 
* which at creation time can let its subclasses decide which class to instantiate. We call this a Factory Pattern
* since it is responsible for "Manufacturing" an Object. It helps instantiate the appropriate Subclass by creating the 
* right Object from a group of related classes. The Factory Pattern promotes loose coupling by eliminating the need to 
* bind application-specific classes into the code. 
*
* Factories have a simple function: Churn out objects.
*
* Obviously, a factory is not needed to make an object. A simple call to new will do it for you. However, 
* the use of factories gives the programmer the opportunity to abstract the specific attributes of an Object 
* into specific subclasses which create them.
*
* The Factory Pattern is all about "Define an interface for creating an object, but let the subclasses decide which 
* class to instantiate. The Factory method lets a class defer instantiation to subclasses" Thus, as defined by 
* the baseclass et al, "The Factory Method lets a class defer instantiation to subclasses".
*/

#include <map>

/**
* @def	FACTORY_REGISTER(factoryName,className)
* 
* @brief	Registers a class (className) in a factory (factoryName) and sets a global variable (bool) named 
*			_factoryLocal\<factoryName\>\<classname\>. If successfully registered it is <code>TRUE</code> otherwise  
*			<code>FALSE</code>.
*
*/

#define FACTORY_REGISTER(factoryName,className)\
	static bool _factoryLocal##factoryName##className = factoryName::instance().registerClass<className>(_T(#className))

#define FACTORY_REGISTER_DWORD(factoryName,className,classID)\
	static bool _factoryLocal##factoryName##className = factoryName::instance().registerClass<className>(classID)


/**
* @fn	BaseClassType * createClassHelper()
*
* @brief	A helper for creating instances of classes contained in the Factory. 
*
* @retval	New instance of a class. 
*/

template<typename BaseClassType , typename ClassType>
BaseClassType * createClassHelper() {
	return new ClassType();
}

/**
* @class FactoryBase
*
* @brief Factory implementation class using c++ templates.
*
* This class implements the Factory pattern. Therefor c++ templating was used.
*
* Factory pattern: 
*
* Factory Method is a creational pattern. This pattern helps to model an interface for creating an object 
* which at creation time can let its subclasses decide which class to instantiate. We call this a Factory Pattern
* since it is responsible for "Manufacturing" an Object. It helps instantiate the appropriate Subclass by creating the 
* right Object from a group of related classes. The Factory Pattern promotes loose coupling by eliminating the need to 
* bind application-specific classes into the code. 
*
* Factories have a simple function: Churn out objects.
*
* Obviously, a factory is not needed to make an object. A simple call to new will do it for you. However, 
* the use of factories gives the programmer the opportunity to abstract the specific attributes of an Object 
* into specific subclasses which create them.
*
* The Factory Pattern is all about "Define an interface for creating an object, but let the subclasses decide which 
* class to instantiate. The Factory method lets a class defer instantiation to subclasses" Thus, as defined by 
* the baseclass et al, "The Factory Method lets a class defer instantiation to subclasses".
*
* To use this factory template you have to do following steps:
* - Implment factory using the template (ConnectionFactory)
* - Create baseclass for containing classes (ConnectionBase)
* - Implement spezific class (Connection1)
* - Use factory
*
* Remarks:
*
* In this example the Factory implementation is also a singleton. Therefor the macro <code>CONNECTION_FACTORY</code> is 
* used to get the one and only instance of the connectionFactory class.
* 
* Implment factory using the template:
*
* @code
#include "FactoryBase.h"
#include "ConnectionBase.h"

#include <string>

#define CONNECTION_FACTORY ConnectionFactory::instance()

class ConnectionFactory :public FactoryBase<ConnectionBase,std::string> {

};	
*
* @endcode
*
* Create baseclass for containing classes:
*
* @code
#include <string>


class ConnectionBase {

public:
virtual std::string getDescription() = 0;

};
* @endcode
*
* Implement spezific class:
*
* @code
#include "String.h"
#include "ConnectionBase.h"
#include "ConnectionFactory.h"


class Connection1 : public ConnectionBase
{
public:
std::string getDescription();
private:
};

FACTORY_REGISTER(ConnectionFactory,Connection1)

* @endcode
*
* Use factory:
*
* @code
ConnectionBase * con = CONNECTION_FACTORY.createClass("Connection1");
* @endcode
*
* @see FACTORY_REGISTER
* @see SingletonBase
*/

template<typename BaseClassType, typename UniqueIdType>
class FactoryBase {
protected:

	/** defines a pointer to a function which returns a pointer to a class of type BaseClassType */
	typedef BaseClassType *(*createClassFunc)();

public:

	/**
	*	ConstFactoryIterator is a const_iterator typedef, for iterating forward and reading only of the registered 
	*   classes of the Factory
	*/
	typedef typename std::map<UniqueIdType, createClassFunc>::const_iterator ConstFactoryIterator;

	/**
	*	FactoryIterator is a iterator typedef like the ConstFactoryIterator, but it has also the capability to change 
	*   the map of classes which the Factory holds.
	*/

	typedef typename std::map<UniqueIdType, createClassFunc>::iterator FactoryIterator;

	/**
	* @fn	template<typename ClassType> bool registerClass(UniqueIdType unique_id)
	*
	* @brief	Registers a class. Afterwards it can be instantiated through the factory. 
	*
	* @param	unique_id	 - Identifier of the class. This itdentifier must be used later to identify
	*							the class which should be instantiated.
	*
	* @retval	bool - true if the registration of the Class succeeds, false if another class was 
	*			registred with this identifier before. 
	*/

	template<typename ClassType> bool registerClass(UniqueIdType unique_id) {
		if (m_object_creator.find(unique_id) != m_object_creator.end()) {
			return false;
		}

		m_object_creator[unique_id] = &createClassHelper<BaseClassType, ClassType>;

		return true;
	}

	/**
	* @fn	bool unregisterClass(UniqueIdType unique_id)
	*
	* @brief	Unregister class. 
	*
	* @param	unique_id	 - Identifier of the class which should be removed from the Factory. 
	*
	* @retval	bool - true if it succeeds, false if it fails. 
	*/

	bool unregisterClass(UniqueIdType unique_id) {
		return (m_object_creator.erase(unique_id) == 1);
	}

	/**
	* @fn	BaseClassType *createClass(UniqueIdType unique_id)
	*
	* @brief	Creates the class. The Type of the class is referenced with the identifier unique_id.
	*
	* @param	unique_id	 - Identifier of the class. 
	*
	* @retval	BaseClassType - null if no class was registered before, else an instance of the requested class. 
	*
	* @sa registerClass
	*/

	BaseClassType * createClass(UniqueIdType unique_id) {
		FactoryIterator iter = m_object_creator.find(unique_id);

		if (iter == m_object_creator.end()){
			return 0;
		}

		return ((*iter).second)();
	}

	/**
	* @fn	ConstFactoryIterator getBegin() const
	*
	* @brief	Gets the begin of the constant iterator. 
	*
	* @retval	ConstFactoryIterator
	*/

	ConstFactoryIterator getBegin() const {
		return m_object_creator.begin();
	}

	/**
	* @fn	ConstFactoryIterator getEnd() const
	*
	* @brief	Gets the end of the constant iterator. 
	*
	* @retval	ConstFactoryIterator 
	*/

	ConstFactoryIterator getEnd() const {
		return m_object_creator.end();
	}

	/**
	* @fn	FactoryIterator getBegin()
	*
	* @brief	Gets the begin of the classes hold by the Factory. 
	*
	* @retval	FactoryIterator 
	*/

	FactoryIterator getBegin() {
		return m_object_creator.begin();
	}

	/**
	* @fn	FactoryIterator getEnd()
	*
	* @brief	Gets the end of the classes hold by the Factory.  
	*
	* @retval	FactoryIterator. 
	*/

	FactoryIterator getEnd() {
		return m_object_creator.end();
	}

protected:

	/** the map which holds the unique identifier and the pointer to a creator function of that spezific class.*/
	std::map<UniqueIdType, createClassFunc> m_object_creator;
};

#endif /* _OGON_SMGR_FACTORYBASE_H_ */
