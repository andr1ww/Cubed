#include "pch.h"

#include "Globals.h"

UFortCreativeRealEstatePlotItem* UFortMcpProfileCreative::FindPlotById(FString PlotId)
{
    auto State = InternalGetState();
    UE_LOG(LogServer, Log, "ItemContainerNum: %d", State->ItemsContainer.Items.Num());
        
    if (State->ItemsContainer.Items.Num() > 0)
    {
        for (auto& Item : State->ItemsContainer.Items)
        {
            if (auto McpItem = Item.Value().Get())
            {
                UE_LOG(LogServer, Log, "Item: %s, Count: %d", McpItem->TemplateId.ToString().c_str(), McpItem->Quantity);
                if (McpItem->InstanceId == PlotId)
                    return Cast<UFortCreativeRealEstatePlotItem>(McpItem->Instance);
            }
        }
    }

    return nullptr;
}
