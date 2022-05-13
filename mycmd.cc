#include <config.h>

#include <asiolink/asio_wrapper.h>
#include <asiolink/io_service.h>
#include <asiolink/tls_socket.h>
#include <cc/command_interpreter.h>
#include <dhcp/pkt4.h>
#include <dhcpsrv/cfgmgr.h>
#include <hooks/hooks.h>
#include <http/client.h>
#include <http/response_json.h>

#include "mycmd_log.h"

using namespace isc;

namespace isc {
    namespace mycmd {
        asiolink::IOServicePtr aio;
        http::HttpClientPtr hcli;
    }
}

extern "C" {
    int version() {
        return KEA_HOOKS_VERSION;
    }

    int multi_threading_compatible() {
        return 0;
    }

    int dhcp4_srv_configured(hooks::CalloutHandle& ctx) {
        LOG_INFO(mycmd::mycmd_logger, MYCMD_MOD_ENTER_PHASE).arg("dhcp4_srv_configured");
        ctx.getArgument("io_context", mycmd::aio);
        mycmd::hcli = boost::make_shared<http::HttpClient>(*(mycmd::aio));
        mycmd::hcli->start();
        LOG_INFO(mycmd::mycmd_logger, MYCMD_MOD_LEAVE_PHASE).arg("dhcp4_srv_configured");
        return 0;
    }

    int list(hooks::CalloutHandle& ctx) {
        data::ConstElementPtr resp;
        data::ElementPtr payload = data::Element::createMap();
        data::ElementPtr subnets4 = data::Element::createList();
        dhcp::SrvConfigPtr cfg = dhcp::CfgMgr::instance().getCurrentCfg();
        dhcp::ConstCfgSubnets4Ptr subnets4ptr = cfg->getCfgSubnets4();
        for (auto const &subnet : *(subnets4ptr->getAll())) {
            data::ElementPtr m = data::Element::createMap();
            m->set("id", data::Element::create(static_cast<long long>(subnet->getID())));
            m->set("subnet", data::Element::create(subnet->toText()));
            subnets4->add(m);
        }
        payload->set("subnets", subnets4);
        resp = config::createAnswer(config::CONTROL_RESULT_SUCCESS, payload);
        ctx.setArgument("response", resp);
        return 0;
    }

    int get(hooks::CalloutHandle& ctx) {
        data::ConstElementPtr req, args, resp;
        dhcp::SrvConfigPtr cfg = dhcp::CfgMgr::instance().getCurrentCfg();
        dhcp::ConstCfgSubnets4Ptr subnets4ptr = cfg->getCfgSubnets4();
        try {
            ctx.getArgument("command", req);
            config::parseCommandWithArgs(args, req);
            data::ConstElementPtr ptr = args->get("id");
            dhcp::SubnetID subnet4id = static_cast<uint32_t>(ptr->intValue());
            dhcp::ConstSubnet4Ptr subnet4 = subnets4ptr->getBySubnetId(subnet4id);
            data::ElementPtr payload = data::Element::createMap();
            data::ElementPtr subnets4 = data::Element::createList();
            subnets4->add(subnet4->toElement());
            payload->set("subnets", subnets4);
            resp = config::createAnswer(config::CONTROL_RESULT_SUCCESS, payload);
        } catch (const std::exception& ex) {
            resp = config::createAnswer(config::CONTROL_RESULT_ERROR, ex.what());
        }
        ctx.setArgument("response", resp);
        return 0;
    }

    int call(hooks::CalloutHandle& ctx) {
        data::ConstElementPtr resp;
        http::Url url("http://172.16.0.10:80/cmdb/v1/machines");
        http::HttpRequestPtr hreq = boost::make_shared<http::HttpRequest>(http::HttpRequest::Method::HTTP_GET, url.getPath(), http::HttpVersion::HTTP_11(), http::HostHttpHeader("172.16.0.10"));
        hreq->finalize();
        http::HttpResponseJsonPtr jresp = boost::make_shared<http::HttpResponseJson>();
        mycmd::hcli->asyncSendRequest(url, asiolink::TlsContextPtr(), hreq, jresp,
                [](const boost::system::error_code& code, const http::HttpResponsePtr& ptr, const std::string& msg){
                LOG_INFO(mycmd::mycmd_logger, MYCMD_LOG_HTTP_CB).arg(code.value()).arg(static_cast<uint16_t>(ptr->getStatusCode()));
        });
        do {
            mycmd::aio->run_one();
            LOG_INFO(mycmd::mycmd_logger, MYCMD_LOG_HTTP_POLL);
        } while(!(jresp->isFinalized()));
        resp = config::createAnswer(config::CONTROL_RESULT_SUCCESS, jresp->getBodyAsJson());
        ctx.setArgument("response", resp);
        return 0;
    }

    int load(hooks::LibraryHandle &ctx) {
        ctx.registerCommandCallout("subnet4-list", list);
        ctx.registerCommandCallout("subnet4-get", get);
        ctx.registerCommandCallout("http-call", call);
        LOG_INFO(mycmd::mycmd_logger, MYCMD_MOD_LOAD);
        return 0;
    }

    int unload() {
        LOG_INFO(mycmd::mycmd_logger, MYCMD_MOD_UNLOAD);
        return 0;
    }
}
