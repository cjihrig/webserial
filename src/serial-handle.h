#include <node_api.h>
#include <libserialport.h>

class SerialHandle {
  public:
    static napi_status Init(napi_env env);
    static void Destructor(napi_env env,
                           void* native_object,
                           void* finalize_hint);
    static napi_status NewInstance(napi_env env,
                                   struct sp_port* port,
                                   napi_value* instance);
    sp_return close_port(void);
    sp_return open_port(int baud_rate,
                        int data_bits,
                        int stop_bits,
                        int parity,
                        int flow_control);
    sp_return get_signals(int* cts, int* dsr, int* dcd, int* ri);
    sp_return set_signals(int dtr, int rts, int brk);
    sp_return read_data(void* buf, size_t size);
    sp_return write_data(void* buf, size_t size);
    sp_return discard_rx_buffer(void);
    sp_return discard_tx_buffer(void);
    sp_return flush_tx_buffer(void);
    void set_port(struct sp_port* port);

  private:
    SerialHandle();
    ~SerialHandle();

    static napi_value New(napi_env env, napi_callback_info info);
    static napi_ref constructor;
    napi_env env_;
    napi_ref wrapper_;
    struct sp_port* port_;
};
