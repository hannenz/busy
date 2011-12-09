--
-- MySQL 5.1.49
-- Fri, 09 Dec 2011 10:52:18 +0000
--

CREATE TABLE `backups` (
   `id` int(11) not null auto_increment,
   `type` varchar(64),
   `queued` datetime,
   `started` datetime,
   `finished` datetime,
   `state` varchar(64),
   `duration` int(11),
   `size` bigint(20),
   `name` varchar(255),
   `path` varchar(255),
   `failures` int(11),
   `host_id` int(11),
   PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=4377;


CREATE TABLE `hosts` (
   `id` int(11) not null auto_increment,
   `name` varchar(255),
   `hostname` varchar(255),
   `user` varchar(255),
   `ip` varchar(20),
   `max_incr` int(11),
   `max_age` float,
   `max_age_full` float,
   `max_age_incr` float,
   `rsync_opts` varchar(255),
   `backupdir` varchar(255),
   `schedule` text,
   `excludes` text,
   `ips` text,
   `srcdirs` text,
   `archivedir` varchar(255),
   `email` varchar(255),
   PRIMARY KEY (`id`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=209;


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
) ENGINE=MyISAM DEFAULT CHARSET=latin1 AUTO_INCREMENT=2975;
