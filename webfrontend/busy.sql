--
-- MySQL 5.1.49
-- Fri, 02 Dec 2011 10:53:13 +0000
--

CREATE TABLE `backups` (
   `id` int(11) not null auto_increment,
   `type` varchar(64),
   `host_name` varchar(100),
   `host_hostname` varchar(100),
   `host_ip` varchar(32),
   `queued` datetime,
   `started` datetime,
   `finished` datetime,
   `state` varchar(64),
   `duration` int(11),
   `size` bigint(20),
   `name` varchar(255),
   `path` varchar(255),
   `host_rsync_opts` varchar(255),
   `host_backupdir` varchar(255),
   `failures` int(11),
   PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=3627;


CREATE TABLE `hosts` (
   `id` int(11) not null auto_increment,
   `name` varchar(255),
   `hostname` varchar(255),
   `ip` varchar(20),
   `max_incr` int(11),
   `max_age_full` float,
   `max_age_incr` float,
   `rsync_opts` varchar(255),
   `backupdir` varchar(255),
   PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=25;


CREATE TABLE `jobs` (
   `id` int(11) not null auto_increment,
   `pid` int(11),
   `srcdir` varchar(255),
   `destdir` varchar(100),
   `backup_id` int(11),
   `rsync_cmd` text,
   `exit_code` int(11),
   `started` datetime,
   `finished` datetime,
   `state` varchar(64),
   PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=2582;
