#include "serial-handle.h"

napi_ref SerialHandle::constructor;

#define RETURN_ON_ERROR(result)                                               \
  do {                                                                        \
    if ((result) != SP_OK) {                                                  \
      return (result);                                                        \
    }                                                                         \
  } while(0)

SerialHandle::SerialHandle() {
  env_ = nullptr;
  wrapper_ = nullptr;
  port_ = nullptr;
}

SerialHandle::~SerialHandle() {
  if (port_ != nullptr) {
    sp_close(port_);
    sp_free_port(port_);
    port_ = nullptr;
  }

  napi_delete_reference(env_, wrapper_);
}

napi_status SerialHandle::Init(napi_env env) {
  napi_status status;
  napi_value cons;

  status = napi_define_class(env,
                             "SerialHandle",
                             -1,
                             New,
                             nullptr,
                             0,
                             nullptr,
                             &cons);
  if (status != napi_ok) {
    return status;
  }

  status = napi_create_reference(env, cons, 1, &constructor);
  if (status != napi_ok) {
    return status;
  }

  return napi_ok;
}

napi_value SerialHandle::New(napi_env env, napi_callback_info info) {
  napi_value self = nullptr;
  napi_status status;
  size_t argc = 0;

  status = napi_get_cb_info(env, info, &argc, nullptr, &self, nullptr);
  if (status != napi_ok) {
    return self;
  }

  SerialHandle* obj = new SerialHandle();
  status = napi_wrap(env,
                     self,
                     obj,
                     SerialHandle::Destructor,
                     nullptr,
                     &obj->wrapper_);
  if (status != napi_ok) {
    return self;
  }

  obj->env_ = env;

  return self;
}

void SerialHandle::Destructor(napi_env env,
                              void* native_object,
                              void* finalize_hint) {
  SerialHandle* obj = static_cast<SerialHandle*>(native_object);
  delete obj;
}

napi_status SerialHandle::NewInstance(napi_env env,
                                      struct sp_port* port,
                                      napi_value* instance) {
  const int argc = 0;
  napi_value cons;
  napi_status status;

  status = napi_get_reference_value(env, constructor, &cons);
  if (status != napi_ok) {
    return status;
  }

  status = napi_new_instance(env, cons, argc, nullptr, instance);
  if (status != napi_ok) {
    return status;
  }

  SerialHandle* handle;
  status = napi_unwrap(env, *instance, reinterpret_cast<void**>(&handle));
  if (status != napi_ok) {
    return status;
  }

  handle->set_port(port);

  return napi_ok;
}

sp_return SerialHandle::close_port(void) {
  sp_return r;

  r = sp_close(port_);
  port_ = nullptr;

  return r;
}

sp_return SerialHandle::open_port(int baud_rate,
                                  int data_bits,
                                  int stop_bits,
                                  int parity,
                                  int flow_control) {
  struct sp_port_config* config;
  sp_return r;

  r = sp_open(port_, SP_MODE_READ_WRITE);
  if (r != SP_OK) {
    return r;
  }

  r = sp_new_config(&config);
  if (r != SP_OK) {
    goto close_port;
  }

  r = sp_set_config_baudrate(config, baud_rate);
  if (r != SP_OK) {
    goto free_config;
  }

  r = sp_set_config_bits(config, data_bits);
  if (r != SP_OK) {
    goto free_config;
  }

  r = sp_set_config_parity(config, (enum sp_parity)parity);
  if (r != SP_OK) {
    goto free_config;
  }

  r = sp_set_config_stopbits(config, stop_bits);
  if (r != SP_OK) {
    goto free_config;
  }

  r = sp_set_config_flowcontrol(config, (enum sp_flowcontrol)flow_control);
  if (r != SP_OK) {
    goto free_config;
  }

  r = sp_set_config(port_, config);

free_config:
  sp_free_config(config);

close_port:
  if (r != SP_OK) {
    sp_close(port_);
  }

  return r;
}

sp_return SerialHandle::get_signals(int* cts, int* dsr, int* dcd, int* ri) {
  sp_signal mask;
  sp_return r;

  r = sp_get_signals(port_, &mask);
  *cts = mask & SP_SIG_CTS;
  *dsr = mask & SP_SIG_DSR;
  *dcd = mask & SP_SIG_DCD;
  *ri = mask & SP_SIG_RI;

  return r;
}

sp_return SerialHandle::set_signals(int dtr, int rts, int brk) {
  if (dtr == 0) {
    RETURN_ON_ERROR(sp_set_dtr(port_, SP_DTR_OFF));
  } else if (dtr == 1) {
    RETURN_ON_ERROR(sp_set_dtr(port_, SP_DTR_ON));
  }

  if (rts == 0) {
    RETURN_ON_ERROR(sp_set_rts(port_, SP_RTS_OFF));
  } else if (rts == 1) {
    RETURN_ON_ERROR(sp_set_rts(port_, SP_RTS_ON));
  }

  if (brk == 0) {
    RETURN_ON_ERROR(sp_end_break(port_));
  } else if (brk == 1) {
    RETURN_ON_ERROR(sp_start_break(port_));
  }

  return SP_OK;
}

sp_return SerialHandle::read_data(void* buf, size_t size) {
  return sp_nonblocking_read(port_, buf, size);
}

sp_return SerialHandle::write_data(void* buf, size_t size) {
  return sp_nonblocking_write(port_, buf, size);
}

sp_return SerialHandle::discard_rx_buffer(void) {
  return sp_flush(port_, SP_BUF_INPUT);
}

sp_return SerialHandle::discard_tx_buffer(void) {
  return sp_flush(port_, SP_BUF_OUTPUT);
}

sp_return SerialHandle::flush_tx_buffer(void) {
  return sp_drain(port_);
}

void SerialHandle::set_port(struct sp_port* port) {
  port_ = port;
}
