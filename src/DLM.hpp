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

#ifndef _DISTRIBUTED_LOCKING_DLM_HPP_
#define _DISTRIBUTED_LOCKING_DLM_HPP_

#include <vector>

#include <fipa_acl/fipa_acl.h>
#include <distributed_locking/Agent.hpp>

namespace fipa {
namespace distributed_locking {
  class DLM
  {
  public:
    // Default constructor
    DLM();
    // Constructor
    DLM(Agent self, std::vector<Agent> agents);
    
    // Adds an agent to the list
    void addAgent(Agent agent);
    
    // Sets self
    void setSelf(Agent self);
    
    // Gets the outgoing messages. Used by the Orogen task.
    std::vector<fipa::acl::ACLMessage>& getOutgoingMessages();
    
    // Tries to locks a resource. Subsequently, isLocked() must be called to check the status.
    virtual void lock(const std::string& resource) = 0;
    // Unlocks a resource, that must have been locked before
    virtual void unlock(const std::string& resource) = 0;
    // Checks if the lock for a given resource is held
    virtual bool isLocked(const std::string& resource) = 0;
    
    // This message is triggered by the wrapping Orogen task, if a message is received
    virtual void onIncomingMessage(const fipa::acl::ACLMessage& message) = 0;
    
    
    protected:
      // The agent represented by this DLM
      Agent self;
      // The agents to communicate with, excluding self
      std::vector<Agent> agents;
      
      // List of outgoing messages. The Orogen task checks in intervals and forwards the messages.
      std::vector<fipa::acl::ACLMessage> outgoingMessages;
  };
  
} // namespace distributed_locking
} // namespace fipa

#endif // _DISTRIBUTED_LOCKING_DLM_HPP_