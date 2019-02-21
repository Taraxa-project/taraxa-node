// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: google/api/expr/v1alpha1/cel_service.proto

#include "google/api/expr/v1alpha1/cel_service.pb.h"
#include "google/api/expr/v1alpha1/cel_service.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/method_handler_impl.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>
namespace google {
namespace api {
namespace expr {
namespace v1alpha1 {

static const char* CelService_method_names[] = {
  "/google.api.expr.v1alpha1.CelService/Parse",
  "/google.api.expr.v1alpha1.CelService/Check",
  "/google.api.expr.v1alpha1.CelService/Eval",
};

std::unique_ptr< CelService::Stub> CelService::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< CelService::Stub> stub(new CelService::Stub(channel));
  return stub;
}

CelService::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_Parse_(CelService_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Check_(CelService_method_names[1], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_Eval_(CelService_method_names[2], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status CelService::Stub::Parse(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::ParseRequest& request, ::google::api::expr::v1alpha1::ParseResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Parse_, context, request, response);
}

void CelService::Stub::experimental_async::Parse(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::ParseRequest* request, ::google::api::expr::v1alpha1::ParseResponse* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Parse_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::google::api::expr::v1alpha1::ParseResponse>* CelService::Stub::AsyncParseRaw(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::ParseRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::google::api::expr::v1alpha1::ParseResponse>::Create(channel_.get(), cq, rpcmethod_Parse_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::google::api::expr::v1alpha1::ParseResponse>* CelService::Stub::PrepareAsyncParseRaw(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::ParseRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::google::api::expr::v1alpha1::ParseResponse>::Create(channel_.get(), cq, rpcmethod_Parse_, context, request, false);
}

::grpc::Status CelService::Stub::Check(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::CheckRequest& request, ::google::api::expr::v1alpha1::CheckResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Check_, context, request, response);
}

void CelService::Stub::experimental_async::Check(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::CheckRequest* request, ::google::api::expr::v1alpha1::CheckResponse* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Check_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::google::api::expr::v1alpha1::CheckResponse>* CelService::Stub::AsyncCheckRaw(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::CheckRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::google::api::expr::v1alpha1::CheckResponse>::Create(channel_.get(), cq, rpcmethod_Check_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::google::api::expr::v1alpha1::CheckResponse>* CelService::Stub::PrepareAsyncCheckRaw(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::CheckRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::google::api::expr::v1alpha1::CheckResponse>::Create(channel_.get(), cq, rpcmethod_Check_, context, request, false);
}

::grpc::Status CelService::Stub::Eval(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::EvalRequest& request, ::google::api::expr::v1alpha1::EvalResponse* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_Eval_, context, request, response);
}

void CelService::Stub::experimental_async::Eval(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::EvalRequest* request, ::google::api::expr::v1alpha1::EvalResponse* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_Eval_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::google::api::expr::v1alpha1::EvalResponse>* CelService::Stub::AsyncEvalRaw(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::EvalRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::google::api::expr::v1alpha1::EvalResponse>::Create(channel_.get(), cq, rpcmethod_Eval_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::google::api::expr::v1alpha1::EvalResponse>* CelService::Stub::PrepareAsyncEvalRaw(::grpc::ClientContext* context, const ::google::api::expr::v1alpha1::EvalRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::google::api::expr::v1alpha1::EvalResponse>::Create(channel_.get(), cq, rpcmethod_Eval_, context, request, false);
}

CelService::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CelService_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CelService::Service, ::google::api::expr::v1alpha1::ParseRequest, ::google::api::expr::v1alpha1::ParseResponse>(
          std::mem_fn(&CelService::Service::Parse), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CelService_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CelService::Service, ::google::api::expr::v1alpha1::CheckRequest, ::google::api::expr::v1alpha1::CheckResponse>(
          std::mem_fn(&CelService::Service::Check), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      CelService_method_names[2],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< CelService::Service, ::google::api::expr::v1alpha1::EvalRequest, ::google::api::expr::v1alpha1::EvalResponse>(
          std::mem_fn(&CelService::Service::Eval), this)));
}

CelService::Service::~Service() {
}

::grpc::Status CelService::Service::Parse(::grpc::ServerContext* context, const ::google::api::expr::v1alpha1::ParseRequest* request, ::google::api::expr::v1alpha1::ParseResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CelService::Service::Check(::grpc::ServerContext* context, const ::google::api::expr::v1alpha1::CheckRequest* request, ::google::api::expr::v1alpha1::CheckResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status CelService::Service::Eval(::grpc::ServerContext* context, const ::google::api::expr::v1alpha1::EvalRequest* request, ::google::api::expr::v1alpha1::EvalResponse* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


}  // namespace google
}  // namespace api
}  // namespace expr
}  // namespace v1alpha1

