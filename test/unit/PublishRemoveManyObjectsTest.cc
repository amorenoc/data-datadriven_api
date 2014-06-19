/******************************************************************************
 * Copyright (c) 2009-2013, AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#include "Common.h"
#include "gtest/gtest.h"

/**
 * Publishing / Removing many objects test.
 */
namespace test_unit_publishremovemanyobjects {
using namespace::test_unit_common;

/**
 * \test The test aims at verifying the ability to publish and remove several objects.
 *       The SimpleTestObject is used in this scenario.
 *       Calling methods on already removed objects should return an error code however it should be possible to access their properties:
 *       -# Publish many test objects after a valid bus connection, observer and listener are set-up
 *       -# Verify that all test objects were published (OnUpdate was called for all)
 *       -# Verify as a consumer that properties of all test objects are accessible and that methods can be invoked with the correct reply/result
 *       -# Remove all published tests objects
 *       -# Verify that all test objects were correctly removed as a consumer (OnRemove was called for all)
 *       -# Verify that properties of all removed test objects are still accessible and that method calls cannot be carried out
 * */
TEST(PublishRemoveManyObjects, Default) {
    int numPubObjs = 500; //Increase for stress testing
    BusConnection bconn;
    std::vector<unique_ptr<TestObject> > tos;
    qcc::String testObjName;

    ASSERT_TRUE(bconn.GetStatus() == ER_OK);

    TestObjectListener testObjectListener = TestObjectListener();
    Observer<SimpleTestObjectProxy> obs(bconn, &testObjectListener);

    ASSERT_TRUE(obs.GetStatus() == ER_OK);

    for (int i = 0; i < numPubObjs; i++) {
        testObjName = ("TestObject" + to_string(i)).c_str();
        tos.push_back(unique_ptr<TestObject>(new TestObject(bconn, testObjName)));
        ASSERT_TRUE(tos.back()->UpdateAll() == ER_OK);
    }

    usleep(numPubObjs * 3000);

    for (int i = 0; i < numPubObjs; i++) {
        testObjName = ("TestObject" + to_string(i)).c_str();
        std::map<qcc::String,
                 std::shared_ptr<SimpleTestObjectProxy> >::iterator itr = testObjectListener.nameToObjects.find(
            testObjName);
        EXPECT_TRUE(itr != testObjectListener.nameToObjects.end());
        EXPECT_TRUE(testObjName.compare(itr->first) == 0);
    }

    /*
     * Check that method calls on published objects valid and that
     * properties are accessible.
     */

    std::map<qcc::String,
             std::shared_ptr<SimpleTestObjectProxy> >::iterator it = testObjectListener.nameToObjects.begin();
    for (; it != testObjectListener.nameToObjects.end(); ++it) {
        SimpleTestObjectProxy::Properties prop = it->second->GetProperties();
        EXPECT_TRUE(it->first.compare(prop.name) == 0);

        std::shared_ptr<datadriven::MethodInvocation<SimpleTestObjectProxy::ExecuteReply> > invocation =
            it->second->Execute();
        SimpleTestObjectProxy::ExecuteReply reply = invocation->GetReply();
        EXPECT_TRUE(reply.GetStatus() == ER_OK);
        EXPECT_TRUE(reply.success);
    }
    //*******************************************************************************************//

    for (auto& i : tos) {
        i->RemoveFromBus();
    }

    usleep(numPubObjs * 3000);

    EXPECT_TRUE((int)testObjectListener.removedObjectNames.size() == numPubObjs);

    /*
     * Check that method calls on removed objects are returning a propper error code and that
     * properties are still accessible.
     */

    it = testObjectListener.nameToObjects.begin();
    for (; it != testObjectListener.nameToObjects.end(); ++it) {
        SimpleTestObjectProxy::Properties prop = it->second->GetProperties();
        EXPECT_TRUE(it->first.compare(prop.name) == 0);

        std::shared_ptr<datadriven::MethodInvocation<SimpleTestObjectProxy::ExecuteReply> > invocation =
            it->second->Execute();
        SimpleTestObjectProxy::ExecuteReply reply = invocation->GetReply();
        EXPECT_TRUE(reply.GetStatus() != ER_OK);
    }
    //*******************************************************************************************//
}
}
//namespace
