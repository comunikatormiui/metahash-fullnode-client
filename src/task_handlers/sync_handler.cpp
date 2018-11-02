#include "sync_handler.h"
#include <memory>
#include "task_handlers.h"
#include "../http_json_rpc_request.h"
#include "../wallet_storage/wallet_storage.h"
#include "../log/log.h"

void base_sync_handler::execute()
{
    BGN_TRY
    {
        executeImpl();
    }
    END_TRY
}