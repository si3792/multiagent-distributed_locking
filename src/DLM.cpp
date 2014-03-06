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

#include "DLM.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>
#include <stdexcept>

namespace ricart_agrawala
{

DLM::DLM()
{
}

DLM::DLM(Agent self, std::vector<Agent> agents)
  : self(self)
  , agents(agents)
{
}

void DLM::addAgent(Agent agent)
{
  agents.push_back(agent);
}

void DLM::setSelf(Agent self)
{
  this->self = self;
}

std::vector<fipa::acl::ACLMessage>& DLM::getOutgoingMessages()
{
  return outgoingMessages;
}

//FIXME non-blocking
// FIXME const ref
void DLM::lock(const std::string& resource)
{
  using namespace fipa::acl;
  // Send a message to everyone, requesting the lock
  ACLMessage message;
  // A simple request
  message.setPerformative(ACLMessage::REQUEST);
  
  // Our request messages are in the format "TIME\nRESOURCE_IDENT"
  base::Time time = base::Time::now();
  message.setContent(time.toString() + "\n" + resource);
  // Add sender and receivers
  message.setSender(AgentID(self.identifier));
  for(unsigned int i = 0; i < agents.size(); i++)
  {
    message.addReceiver(AgentID(agents[i].identifier));
  }
  
  //message.addReplyTo(AgentID("sender-agent"));
  //message.setInReplyTo("in-reply-to-nothing");
  //message.setReplyBy(base::Time::now());
  
  // Set and increase conversation ID
  message.setConversationID(self.identifier + boost::lexical_cast<std::string>(conversationIDnum));
  conversationIDnum++;
  message.setProtocol("ricart_agrawala");
  //message.setLanguage("a-content-language");
  //message.setEncoding("encoding-of-content");
  //message.setOntology("an-ontology");
  
  // Add to outgoing messages and to interest list
  outgoingMessages.push_back(message);
  interests[resource] = time;
  
  // Now a response from each agent must be received before we can enter the critical section
  // TODO
}

void DLM::unlock(std::string resource)
{
  // Send all deferred messages for that resource
  sendAllDeferredMessages(resource);
}


void DLM::onIncomingMessage(fipa::acl::ACLMessage message)
{
  using namespace fipa::acl;
  // Check message type
  if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::REQUEST)
  {
    handleIncomingRequest(message);
  }
  else if(ACLMessage::performativeFromString(message.getPerformative()) == ACLMessage::INFORM)
  {
    handleIncomingResponse(message);
  }
  // We ignore other performatives, as they are not part of our protocol.
}

void DLM::handleIncomingRequest(fipa::acl::ACLMessage message)
{
  using namespace fipa::acl;
  
  base::Time otherTime;
  std::string resource;
  extractInformation(message, otherTime, resource);  
  
  // Create a response
  ACLMessage response;
  // An inform we're not interested in or done using the resource
  response.setPerformative(ACLMessage::INFORM);
  
  // Add sender and receiver
  response.setSender(AgentID(self.identifier));
  response.addReceiver(message.getSender());
  
  // Keep the conversation ID
  response.setConversationID(message.getConversationID());
  response.setProtocol("ricart_agrawala");
  
  // TODO what in the case of same time??
  // We send this message now, if we don't hold the resource, are not interested or have been slower. Otherwise we defer it.
  if(std::find(heldResources.begin(), heldResources.end(), resource) == heldResources.end() 
    || interests.count(resource) == 0 || otherTime < interests.at(resource))
  {
    // Our response messages are in the format "TIME\nRESOURCE_IDENT"
    response.setContent(base::Time::now().toString() +"\n" + resource);
    outgoingMessages.push_back(response);
  }
  else
  {
    // We will have to add the timestamp later!
    response.setContent(resource);
    deferredMessages.push_back(response);
  }
}

void DLM::handleIncomingResponse(fipa::acl::ACLMessage message)
{
  using namespace fipa::acl;
  // TODO
}

void DLM::extractInformation(fipa::acl::ACLMessage message, base::Time& time, std::string& resource)
{
  // Split by newline
  std::vector<std::string> strs;
  std::string s = message.getContent();
  boost::split(strs, s, boost::is_any_of("\n")); // XXX why do I need to put it in an extra string?
  
  // TODO check vector size
  //throw std::runtime_error("DLM::extractInformation ...")
  
  time = base::Time::fromString(strs[0]);
  resource = strs[1];
}

void DLM::sendAllDeferredMessages(std::string resource)
{
  for(unsigned int i = 0; i < deferredMessages.size(); i++)
  {
    fipa::acl::ACLMessage msg = deferredMessages[i];
    if(msg.getContent() != resource)
    {
      // Ignore deferred messages for other resources
      continue;
    }
    // Include timestamp
    msg.setContent(base::Time::now().toString() +"\n" + msg.getContent());
    // Remove from deferredMessages, decrement i and add to outgoingMessages
    deferredMessages.erase(deferredMessages.begin() + i);
    i--;
    outgoingMessages.push_back(msg);
  }
}


} //namespace ricart_agrawala