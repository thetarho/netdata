# Baseten Plugin Quick Start Guide

## 🚀 Quick Build & Test

### 1. Install Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install libcurl4-openssl-dev libjson-c-dev cmake

# RHEL/CentOS
sudo yum install libcurl-devel json-c-devel cmake

# macOS
brew install curl json-c cmake
```

### 2. Build the Plugin

```bash
cd /Users/anirudhr/Desktop/test/thetarho/netdata/src/collectors/baseten.plugin

mkdir build && cd build
cmake ..
make

# You should see: baseten.plugin executable created
```

### 3. Configure

Edit `/etc/netdata/netdata.conf` (or create it):

```ini
[plugins]
    baseten = yes

[plugin:baseten]
    api key = YOUR_BASETEN_API_KEY_HERE
    update every = 60
    timeout = 30
```

### 4. Test Manually

```bash
# Set API key
export NETDATA_BASETEN_API_KEY="your_api_key"

# Run plugin directly
cd build
./baseten.plugin

# You should see:
# BASETEN: Successfully fetched initial data
# FUNCTION GLOBAL "deployments" ...
```

### 5. Install

```bash
sudo cp baseten.plugin /usr/libexec/netdata/plugins.d/
sudo chmod 755 /usr/libexec/netdata/plugins.d/baseten.plugin
sudo chown root:netdata /usr/libexec/netdata/plugins.d/baseten.plugin
sudo systemctl restart netdata
```

### 6. Verify

```bash
# Check if plugin is running
ps aux | grep baseten.plugin

# Test the function
curl 'http://localhost:19999/api/v3/function?function=deployments&format=json'

# Check logs
sudo journalctl -u netdata -f | grep BASETEN
```

### 7. Access UI

1. Open browser: http://localhost:19999
2. Navigate to **Functions** tab
3. Look for **"Baseten Model Deployments"**
4. Click to see your interactive table!

## 📊 Expected Output

The table will show:

| Model Name | Model ID | Deployment | Instance Type | Environment | Status | Prod | Dev | Replicas |
|------------|----------|------------|---------------|-------------|--------|------|-----|----------|
| Llama3.1-Aloe | nwxoo97w | deployment-1 | H100x26x234 | production | ACTIVE | Yes | No | 1 |
| gpt-oss-20b | 2qj68xgw | deployment-2 | H100x26x234 | production | SCALED_TO_ZERO | Yes | No | 0 |

With:
- ✅ Color-coded rows (green=active, yellow=scaled down, red=failed)
- ✅ Filterable by environment and status
- ✅ Searchable by model and deployment name
- ✅ Sortable by any column

## 🐛 Troubleshooting

### Plugin Won't Start

```bash
# Check API key
curl -H "Api-Key: YOUR_KEY" https://api.baseten.co/v1/models

# Test plugin manually
sudo su -s /bin/bash netdata
/usr/libexec/netdata/plugins.d/baseten.plugin
```

### No Data in Table

```bash
# Check Baseten account has deployments
curl -H "Api-Key: YOUR_KEY" https://api.baseten.co/v1/models

# Check plugin logs
sudo journalctl -u netdata -f | grep BASETEN
```

### Build Errors

```bash
# Verify dependencies
pkg-config --cflags --libs libcurl
pkg-config --cflags --libs json-c

# Check Netdata headers
ls /Users/anirudhr/Desktop/test/thetarho/netdata/src/libnetdata/
```

## 🎯 Common Tasks

### Change Update Interval

Edit netdata.conf:
```ini
[plugin:baseten]
    update every = 120  # 2 minutes
```

### Disable Plugin

```ini
[plugins]
    baseten = no
```

### Debug Mode

```bash
# Enable verbose logging
export NETDATA_DEBUG=1
./baseten.plugin
```

## 📁 File Locations

```
/Users/anirudhr/Desktop/test/thetarho/netdata/src/collectors/baseten.plugin/
├── baseten-internals.h          # Header file
├── baseten-main.c                # Main entry point
├── baseten-api.c                 # API client
├── baseten-function.c            # Table function
├── baseten-utils.c               # Utilities
├── CMakeLists.txt                # Build config
├── README.md                     # Full documentation
├── IMPLEMENTATION_SUMMARY.md     # Implementation details
├── PLUGIN_INTEGRATION.md         # Docker & custom builds
├── baseten.conf.example          # Config example
└── QUICK_START.md                # This file

After installation:
/usr/libexec/netdata/plugins.d/baseten.plugin  # Plugin binary
/etc/netdata/netdata.conf                       # Configuration
```

## 🔗 Resources

- **Baseten API Docs**: https://docs.baseten.co/api-reference/overview
- **Netdata Functions**: See `FUNCTION_UI_DEVELOPER_GUIDE.md`
- **Get API Key**: https://app.baseten.co/settings/api-keys

## ✅ Success Checklist

- [ ] Dependencies installed (libcurl, json-c)
- [ ] Plugin compiles without errors
- [ ] API key configured in netdata.conf
- [ ] Plugin starts successfully
- [ ] Can fetch data from Baseten API
- [ ] Function registered in Netdata
- [ ] Table appears in UI
- [ ] Filters and sorting work
- [ ] Row colors display correctly

---

**Need Help?** Check the full [README.md](README.md) or [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) for detailed information.
