#include <hooks/hooks.h>
#include <http/client.h>
#include <http/response_json.h>

#include "hb0715_log.h"

using namespace isc;

extern "C" {
    int version() {
        return KEA_HOOKS_VERSION;
    }

    int multi_threading_compatible() {
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
        asiolink::IOServicePtr ev = boost::make_shared<asiolink::IOService>();
        http::HttpClientPtr cli = boost::make_shared<http::HttpClient>(*ev);
        cli->asyncSendRequest(url, asiolink::TlsContextPtr(), hreq, jresp,
                [&ev](const boost::system::error_code& code, const http::HttpResponsePtr& ptr, const std::string& msg){
                LOG_INFO(mycmd::mycmd_logger, MYCMD_LOG_HTTP_CB).arg(code).arg(ptr->getStatusCode());
                ev->stopWork();
                });
        ev->run();
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