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

#ifndef _RICART_AGRAWALA_DLM_HPP_
#define _RICART_AGRAWALA_DLM_HPP_

#include <vector>
#include <map>

#include <fipa_acl/fipa_acl.h>
#include <ricart_agrawala/Agent.hpp>

// TODO switch from vector to set where possible

namespace ricart_agrawala
{
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
    
    // Locks a resource, blocking until access is granted
    void lock(const std::string& resource);
    // Unlocks a resource, that must have been locked before
    void unlock(std::string resource);
    
    // This message is triggered by the wrapping Orogen task, if a message is received
    void onIncomingMessage(fipa::acl::ACLMessage message);
    
    // Gets the outgoing messages. Used by the Orogen task.
    std::vector<fipa::acl::ACLMessage>& getOutgoingMessages();
    
  private:
    // The agent represented by this DLM
    Agent self;
    // The agents to communicate with, excluding self
    std::vector<Agent> agents;
    
    // List of outgoing messages. The Orogen task checks in intervals and forwards the messages.
    std::vector<fipa::acl::ACLMessage> outgoingMessages;
    // Messages to be sent later, by leaving the associated critical resource
    std::vector<fipa::acl::ACLMessage> deferredMessages;
    // Current number for conversation IDs
    int conversationIDnum;
    // All current interests mapped to the time where the message request was created
    std::map<std::string, base::Time> interests;
    // All critical resources held at the moment
    std::vector<std::string> heldResources;
    
    // Handles an incoming request
    void handleIncomingRequest(fipa::acl::ACLMessage message);
    // Handles an incoming response
    void handleIncomingResponse(fipa::acl::ACLMessage message);
    // Extracts the information from the content and saves it in the passed references
    void extractInformation(fipa::acl::ACLMessage message, base::Time& time, std::string& resource);
    // Sends all deferred messages for a certain resource by putting them into outgoingMessages
    void sendAllDeferredMessages(std::string resource);
  };
}


#endif // _RICART_AGRAWALA_DLM_HPP_
