#include "pch.h"

#include "Offsets.h"

TArray<FGuid> UConversationRegistry::GetConnectedNodeIDs(TArray<FGuid>& NodeIDs)
{
    BuildDependenciesGraph(this);

    TArray<FGuid> ConnectedIDs;

    for (FGuid& NodeID : NodeIDs)
    {
        UConversationDatabase* Database = ConversationFromNodeGUID(this, NodeID);
        if (!Database) continue;

        UConversationNode** NodePtr = Database->ReachableNodeMap.Search([&](FGuid& Guid, UConversationNode*&) {
            return Guid == NodeID;
        });

        if (!NodePtr) continue;

        if (UConversationNodeWithLinks* NodeWithLinks = Cast<UConversationNodeWithLinks>(*NodePtr))
            ConnectedIDs.Append(NodeWithLinks->OutputConnections);
    }

    return ConnectedIDs;
}

TArray<FGuid> UConversationRegistry::GetEntryNodeIDs(FGameplayTag EntryTag)
{
    TArray<FGuid>* EntryList = EntryTagToEntryList.Search([&](FGameplayTag& Tag, TArray<FGuid>&) {
        return Tag.TagName == EntryTag.TagName;
    });
    
    return EntryList ? *EntryList : TArray<FGuid>();
}

TArray<FGuid> UConversationRegistry::GetConnectedNodeIDs(FGameplayTag EntryTag)
{
    TArray<FGuid> EntryNodes = GetEntryNodeIDs(EntryTag);
    return GetConnectedNodeIDs(EntryNodes);
}