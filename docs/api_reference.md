# API Reference

## Header Map

| Header                        | Purpose                         |
| ----------------------------- | ------------------------------- |
| `include/kdal/types.h`        | Core types, enums, structs      |
| `include/kdal/api/common.h`   | Feature flags, status codes     |
| `include/kdal/api/driver.h`   | Conventional driver contract    |
| `include/kdal/api/accel.h`    | Accelerator contract            |
| `include/kdal/backend.h`      | Backend operations table        |
| `include/kdal/ioctl.h`        | Userspace ABI definitions       |
| `include/kdal/core/kdal.h`    | Lifecycle, registration, lookup |
| `include/kdal/core/version.h` | Version constants               |

## Lifecycle Functions

```c
int  kdal_core_init(void);       /* Module init cascade */
void kdal_core_exit(void);       /* Module exit cascade */
int  kdal_chardev_init(void);    /* Register /dev/kdal */
void kdal_chardev_exit(void);    /* Deregister /dev/kdal */
int  kdal_debugfs_init(void);    /* Create debugfs tree */
void kdal_debugfs_exit(void);    /* Remove debugfs tree */
```

## Registry Functions

```c
/* Backend registration */
int  kdal_register_backend(struct kdal_backend *backend);
void kdal_unregister_backend(struct kdal_backend *backend);

/* Driver registration */
int  kdal_register_driver(struct kdal_driver *driver);
void kdal_unregister_driver(struct kdal_driver *driver);

/* Device registration */
int  kdal_register_device(struct kdal_device *device);
void kdal_unregister_device(struct kdal_device *device);

/* Attachment */
int  kdal_attach_driver(struct kdal_device *device, struct kdal_driver *driver);
void kdal_detach_driver(struct kdal_device *device);

/* Lookup */
struct kdal_backend *kdal_find_backend(const char *name);
struct kdal_driver  *kdal_find_driver(enum kdal_device_class class_id);
struct kdal_device  *kdal_find_device(const char *name);

/* Introspection */
void kdal_get_registry_snapshot(struct kdal_registry_snapshot *snapshot);
int  kdal_for_each_device(kdal_device_iter_fn fn, void *data);
```

## Event Functions

```c
int  kdal_emit_event(enum kdal_event_type type, const char *device_name, u32 data);
int  kdal_poll_event(struct kdal_event *out);  /* Returns -EAGAIN if empty */
void kdal_event_shutdown(void);                /* Reset event log */
```

## Power Management

```c
int kdal_set_device_power(struct kdal_device *device, enum kdal_power_state state);
```

Calls the driver's `set_power_state` op if available, updates `device->power_state`,
and emits a `KDAL_EVENT_POWER_CHANGE` event.

## Driver Contract (`struct kdal_driver_ops`)

```c
int     (*probe)(struct kdal_device *device);
void    (*remove)(struct kdal_device *device);
ssize_t (*read)(struct kdal_device *device, char __user *buf, size_t count, loff_t *ppos);
ssize_t (*write)(struct kdal_device *device, const char __user *buf, size_t count, loff_t *ppos);
long    (*ioctl)(struct kdal_device *device, unsigned int cmd, unsigned long arg);
int     (*set_power_state)(struct kdal_device *device, enum kdal_power_state state);
```

## Accelerator Contract (`struct kdal_accel_ops`)

```c
int  (*queue_create)(struct kdal_device *device, struct kdal_accel_queue *queue);
void (*queue_destroy)(struct kdal_device *device, struct kdal_accel_queue *queue);
int  (*buffer_map)(struct kdal_device *device, struct kdal_accel_buffer *buffer);
void (*buffer_unmap)(struct kdal_device *device, struct kdal_accel_buffer *buffer);
int  (*submit)(struct kdal_device *device, struct kdal_accel_queue *queue,
               const void *cmd, size_t cmd_len);
```

## Backend Contract (`struct kdal_backend_ops`)

```c
int     (*init)(struct kdal_backend *backend);
void    (*exit)(struct kdal_backend *backend);
int     (*enumerate)(struct kdal_backend *backend);
ssize_t (*read)(struct kdal_device *device, char *buf, size_t count);
ssize_t (*write)(struct kdal_device *device, const char *buf, size_t count);
long    (*ioctl)(struct kdal_device *device, unsigned int cmd, unsigned long arg);
```

## Userspace ioctl ABI

| Command                   | Direction | Payload              | Description                          |
| ------------------------- | --------- | -------------------- | ------------------------------------ |
| `KDAL_IOCTL_GET_VERSION`  | `_IOR`    | `kdal_ioctl_version` | Module version                       |
| `KDAL_IOCTL_GET_INFO`     | `_IOWR`   | `kdal_ioctl_info`    | Device info (by name or selected)    |
| `KDAL_IOCTL_SET_POWER`    | `_IOW`    | `__u32`              | Power state (0=off, 1=on, 2=suspend) |
| `KDAL_IOCTL_LIST_DEVICES` | `_IOR`    | `kdal_ioctl_list`    | All registered device names          |
| `KDAL_IOCTL_POLL_EVENT`   | `_IOR`    | `kdal_ioctl_event`   | Pop oldest event                     |
| `KDAL_IOCTL_SELECT_DEV`   | `_IOW`    | `kdal_ioctl_devname` | Select device for read/write         |

## Enumerations

```c
enum kdal_device_class { UNSPEC, UART, I2C, SPI, GPIO, GPU, NPU };
enum kdal_power_state  { OFF, ON, SUSPEND };
enum kdal_event_type   { NONE, DEVICE_ADDED, DEVICE_REMOVED, IO_READY,
                         POWER_CHANGE, ACCEL_COMPLETE, FAULT };
```

## Feature Flags

```c
KDAL_FEAT_EVENT_PUSH   BIT(0)   /* Backend supports event push */
KDAL_FEAT_POWER_MGMT   BIT(1)   /* Backend supports power management */
KDAL_FEAT_SHARED_MEM   BIT(2)   /* Backend supports shared memory */
KDAL_FEAT_ACCEL_QUEUE  BIT(3)   /* Backend supports accel queues */
```

## Compiler API (`compiler/include/codegen.h`)

The compiler is available as both a standalone binary (`kdalc`) and a library
(`libkdalc.a`) linked into the `kdality` tool.

### Pipeline Functions

```c
/* Full pipeline: .kdc file → generated .c + Makefile.kbuild */
int kdal_compile_file(const char *src_path, const char *out_dir);

/* Individual stages */
kdal_token_t *kdal_lex(const char *source, size_t len, int *out_count);
kdal_ast_node_t *kdal_parse(kdal_token_t *tokens, int count, kdal_arena_t *arena);
int kdal_sema(kdal_ast_node_t *root, kdal_symtab_t *symtab);
int kdal_generate(kdal_ast_node_t *root, const char *src_path, kdal_codegen_opts_t *opts);
```

### Code Generation Options

```c
typedef struct {
    const char *output_dir;      /* Directory for generated files */
    const char *module_name;     /* Kernel module name override */
    int         emit_kbuild;     /* 1 = generate Makefile.kbuild */
    int         emit_of_table;   /* 1 = generate OF match table */
} kdal_codegen_opts_t;
```

### AST Types (partial)

```c
typedef struct kdal_ast_node {
    enum kdal_ast_kind kind;     /* DEVICE, REGISTER_MAP, HANDLER, ... */
    union { ... };               /* Kind-specific payload */
    struct kdal_ast_node *next;  /* Sibling list */
} kdal_ast_node_t;
```

See `compiler/include/ast.h` for the full AST node hierarchy and
`compiler/include/token.h` for the 80+ token type enumeration.
