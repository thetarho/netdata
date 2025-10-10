# Baseten Model Deployments Plugin

This Netdata plugin monitors Baseten AI model deployments, providing an interactive table showing deployment status, environment configuration, and resource allocation.

## Features

- **Real-time Deployment Monitoring**: Track all Baseten model deployments in a single view
- **Interactive Table**: Filter, sort, and search deployments
- **Visual Status Indicators**: Color-coded rows based on deployment status
- **Environment Filtering**: Filter by production, development, staging, or custom environments
- **Resource Information**: View GPU and resource specifications for each deployment

## Configuration

### Prerequisites

- Baseten API key (get from https://baseten.co)
- libcurl development libraries
- json-c library

### Installation

The plugin is built as part of Netdata. If building from source:

```bash
cd netdata
mkdir build && cd build
cmake ..
make
sudo make install
```

### Configuration File

Add to `netdata.conf`:

```ini
[plugins]
    baseten = yes

[plugin:baseten]
    # REQUIRED: Your Baseten API key
    api key = YOUR_API_KEY_HERE

    # Optional: How often to fetch data from Baseten API (seconds)
    update every = 60

    # Optional: API request timeout (seconds)
    timeout = 30
```

### Getting Your API Key

1. Log in to https://app.baseten.co
2. Navigate to Settings → API Keys
3. Create a new API key or use an existing one
4. Copy the key and add it to your Netdata configuration

## Usage

Once configured, the plugin will:

1. Fetch all models and their deployments from Baseten API
2. Register a function called `deployments` in Netdata
3. Update data every 60 seconds (configurable)

### Accessing the Table

In Netdata Cloud or local UI:

1. Go to the Functions tab
2. Find "Baseten Model Deployments" under Cloud functions
3. Click to open the interactive table

### Table Columns

| Column | Description | Features |
|--------|-------------|----------|
| **Model Name** | Name and version of the deployed model | Searchable, Sticky |
| **Model ID** | Unique identifier for the model | Searchable, Unique Key |
| **Deployment Name** | Deployment instance identifier | Searchable |
| **Instance Type** | Hardware configuration (GPU, VRAM, CPU, RAM) | Full-width display |
| **Environment** | Deployment environment | Filterable pills |
| **Status** | Current operational status | Filterable pills, Color-coded |
| **Production** | Whether this is a production deployment | Filterable |
| **Development** | Whether this is a development deployment | Filterable |
| **Active Replicas** | Number of active instances running | Sortable, Range filter |

### Status Color Coding

- **Green (Normal)**: ACTIVE - Deployment is running normally
- **Yellow (Warning)**: SCALED_TO_ZERO, INACTIVE - Deployment is idle or stopped
- **Red (Error)**: FAILED - Deployment has failed
- **Blue (Notice)**: DEPLOYING - Deployment is in progress

## API Endpoints Used

The plugin uses the following Baseten API endpoints:

- `GET /v1/models` - Fetch all models
- `GET /v1/models/{model_id}/deployments` - Fetch deployments for each model

## Troubleshooting

### Plugin Not Appearing

1. Check if the plugin is enabled in `netdata.conf`
2. Verify the API key is correct
3. Check Netdata logs: `sudo journalctl -u netdata -f | grep BASETEN`

### No Data Shown

1. Verify API key has proper permissions
2. Check network connectivity to `api.baseten.co`
3. Verify you have deployments in your Baseten account

### Debug Mode

Run the plugin manually to see detailed output:

```bash
sudo su -s /bin/bash netdata
cd /usr/libexec/netdata/plugins.d
./baseten.plugin debug
```

## Performance

- **Cache TTL**: 60 seconds (responses are cached)
- **API Calls**: One call per model + 1 for model list
- **Memory**: Minimal - data is freed after each update
- **CPU**: Negligible - only active during API fetches

## Security

- API key is stored in netdata.conf (restrict permissions)
- All API calls use HTTPS with SSL verification
- No sensitive deployment data is logged

## Development

### Building

```bash
cd src/collectors/baseten.plugin
mkdir build && cd build
cmake ..
make
```

### Testing

Set your API key in the environment and run:

```bash
export NETDATA_BASETEN_API_KEY="your_key_here"
./baseten.plugin
```

## License

GPL-3.0-or-later

## Support

- Netdata Community: https://community.netdata.cloud
- Baseten Documentation: https://docs.baseten.co
- GitHub Issues: https://github.com/netdata/netdata/issues
