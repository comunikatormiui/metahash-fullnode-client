#include "get_count_blocks_handler.h"
#include "settings/settings.h"
#include "../SyncSingleton.h"
#include "../generate_json.h"
#include "../sync/BlockInfo.h"
#include "../sync/BlockChainReadInterface.h"
#include "cache/blocks_cache.h"

get_count_blocks_handler::get_count_blocks_handler(session_context_ptr ctx)
    : base_network_handler(settings::server::get_tor(), ctx)
{
    m_duration.set_message(__func__);
    m_name = __func__;
}

bool get_count_blocks_handler::prepare_params()
{
    BGN_TRY
    {
        CHK_REQ(m_id, "id field not found")
        return true;
    }
    END_TRY(return false)
}

void get_count_blocks_handler::execute()
{
    BGN_TRY
    {
        if (settings::system::useLocalDatabase) {
            CHK_PRM(syncSingleton() != nullptr, "Sync not set");
            const torrent_node_lib::Sync &sync = *syncSingleton();
            const size_t countBlocks = sync.getBlockchain().countBlocks();
            genCountBlockJson(countBlocks, false, JsonVersion::V1, m_writer.get_doc());
        } else if (blocks_cache::get()->runing()) {
            genCountBlockJson(blocks_cache::get()->last_signed_block(), false, JsonVersion::V1, m_writer.get_doc());
        } else {
            base_network_handler::execute();
        }
    }
    END_TRY()
}
