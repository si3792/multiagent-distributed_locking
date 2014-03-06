/*
 * Copyright 2014 Satia <email>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _RICART_AGRAWALA_AGENT_HPP_
#define _RICART_AGRAWALA_AGENT_HPP_

#include <string>

namespace ricart_agrawala
{
  /**
   * Very basic agent for mutual exclusion on a distributed system. It only contains an identifier.
   */
  struct Agent
  {
    // Identifier of the agent
    std::string identifier;
    
    // Default Constructor -- required
    Agent()
	    : identifier()
    {   
    }   

    // Constrctor with identifier
    Agent(const std::string& identifier)
	    : identifier(identifier)
    {   
    } 
  };
}



#endif // _RICART_AGRAWALA_AGENT_HPP_
