import os
import json
import logging.config



def setup_logging(
    default_path='cfg/logging.json',
    default_level=logging.INFO,
    env_key='LOG_CFG'):
    """Setup logging configuration

    """
    path = default_path
    value = os.getenv(env_key, None)
    if value:
        path = value
    if os.path.exists(path):
        with open(path, 'rt') as f:
            config = json.load(f)
        logging.config.dictConfig(config)
        logger = logging.getLogger('kv-server')
        logger.info("Initialized logging")
    else:
        logging.basicConfig(level=default_level)
        logger = logging.getLogger('kv-server')
        logger.info("Could not find logging config, falling back to basic logging")
