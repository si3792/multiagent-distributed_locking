#ifndef DISTRIBUTED_LOCKING_AGENT_HPP
#define DISTRIBUTED_LOCKING_AGENT_HPP

#include <string>

namespace fipa {
    /**
     * Very basic agent for mutual exclusion on a distributed system. It only contains an identifier.
     */
    class Agent
    {
    public:
        // Identifier of the agent
        std::string identifier;
        
        /**
         * Default Constructor -- required
         */
        Agent()
                : identifier()
        {   
        }   

        /**
        * Constrctor with identifier
        */
        Agent(const std::string& identifier)
                : identifier(identifier)
        {   
        } 
        
        bool operator < (const Agent& a) const
        {
            return identifier < a.identifier;
        }
        
        bool operator == (const Agent& a) const
        {
            return identifier == a.identifier;
        }
  };
} // namespace fipa



#endif // DISTRIBUTED_LOCKING_AGENT_HPP
